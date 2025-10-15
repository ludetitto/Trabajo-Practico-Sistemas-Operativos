/* -----------------------------------------------------------------------------
 * coordinador.c
 *
 * Rol: Consumidor/Coordinador del Ejercicio 1.
 *
 * Qué hace:
 *   - Abre el CSV (escribe encabezado si corresponde) y consume EXACTAMENTE 'total'
 *     registros desde el buffer circular compartido (orden FIFO).
 *   - Loguea cada escritura a stdout.
 *   - Reacciona a señales de terminación (SIGINT/SIGTERM/SIGHUP) para cerrar ordenado.
 *   - **NUEVO**: Tolerancia a fallos frente a muerte de un generador (SIGCHLD):
 *       * Handler minimalista que notifica por pipe.
 *       * Hilo que reap-ea hijos con waitpid(WNOHANG), marca ids_shm->alive[idx] = 0
 *         bajo protección de sem_ids, y reajusta ids_shm->turno para RR estricto.
 *       * Con esto, aunque mates un generador, el sistema completa 'total' registros
 *         con los generadores restantes.
 * -------------------------------------------------------------------------- */

/* -----------------------------------------------------------------------------
 * coordinador.c  —  Consumidor/Coordinador del Ejercicio 1.
 *
 * - Abre CSV (encabezado desde csv.c), consume EXACTAMENTE 'total' elementos
 *   del buffer circular y los persiste (FIFO).
 * - Maneja terminación ordenada (SIGINT/SIGTERM/SIGHUP).
 * - Tolerancia a fallos: al morir un generador (SIGCHLD) se notifica vía
 *   self-pipe; un hilo reap-ea con waitpid(WNOHANG) y llama a ipc_mark_dead(pid).
 *   Así el sistema continúa con los productores vivos hasta completar 'total'.
 * -------------------------------------------------------------------------- */

#include "../include/coordinador.h"

#include <sys/wait.h> // waitpid
#include <pthread.h>  // hilo para procesar SIGCHLD
#include <unistd.h>   // pipe, read, write, close
#include <fcntl.h>    // fcntl
#include <errno.h>    // errno

/* ==========
 * Señales y coordinación
 * ========== */
static volatile sig_atomic_t g_stop = 0; /* terminar ordenado */
static int sigchld_pipe[2] = {-1, -1};   /* pipe para notificar SIGCHLD al hilo */
static pthread_t sigchld_thread;
static volatile int sigchld_thread_running = 0;

/* Handler de terminación: sólo marca g_stop */
static void on_term(int s)
{
    (void)s;
    g_stop = 1;
}

/* Handler de SIGCHLD (async-signal-safe): sólo escribe 1 byte en el pipe */
static void on_sigchld(int s)
{
    (void)s;
    if (sigchld_pipe[1] != -1)
    {
        char b = 'C';
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
        (void)write(sigchld_pipe[1], &b, 1);
#pragma GCC diagnostic pop
    }
}

/* Reap no bloqueante de todos los hijos terminados y delega a ipc_mark_dead(pid) */
static void process_sigchld_events(void)
{
    int status;
    pid_t cpid;

    while ((cpid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        fprintf(stderr, "[COORD] SIGCHLD: child pid=%d terminated (status=%d)\n",
                (int)cpid, status);

        /* Delegar el marcado de 'alive=0' y ajuste de turno a la capa IPC */
        ipc_mark_dead(cpid);
    }
    /* cpid == 0 => no hay más hijos terminados ahora */
}

/* Hilo lector del pipe: bloquea en read() y, cuando hay datos, llama a process_sigchld_events() */
static void *sigchld_thread_func(void *arg)
{
    (void)arg;
    sigchld_thread_running = 1;

    for (;;)
    {
        char buf[64];
        ssize_t r = read(sigchld_pipe[0], buf, sizeof(buf));
        if (r > 0)
        {
            process_sigchld_events();
        }
        else if (r == 0)
        {
            /* pipe cerrada: salir */
            break;
        }
        else
        {
            if (errno == EINTR)
                continue; /* señal mientras leíamos: reintentar */
            break;        /* otro error: salir del hilo */
        }
        if (!sigchld_thread_running)
            break;
    }

    sigchld_thread_running = 0;
    return NULL;
}

/* Inicializa handlers y el hilo de SIGCHLD; llamar antes del bucle principal */
static void install_signal_handlers_and_thread(void)
{
    /* Handlers de terminación */
    struct sigaction sa = {0};
    sa.sa_handler = on_term;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);

    /* Pipe para notificación SIGCHLD -> hilo */
    if (pipe(sigchld_pipe) == -1)
    {
        perror("[COORD] pipe(sigchld_pipe)");
        /* puede seguir sin tolerancia si falla */
        return;
    }

    /* Lectura bloqueante (conveniente para el hilo) */
    int fl = fcntl(sigchld_pipe[0], F_GETFL, 0);
    fcntl(sigchld_pipe[0], F_SETFL, fl & ~O_NONBLOCK);

    /* Instalar SIGCHLD */
    struct sigaction sc = {0};
    sc.sa_handler = on_sigchld;
    sigemptyset(&sc.sa_mask);
    sc.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sc, NULL) == -1)
    {
        perror("[COORD] sigaction(SIGCHLD)");
        return;
    }

    /* Crear hilo lector del pipe */
    if (pthread_create(&sigchld_thread, NULL, sigchld_thread_func, NULL) != 0)
    {
        perror("[COORD] pthread_create(sigchld_thread)");
        /* seguir sin hilo si falla */
    }
}

/* Apaga el hilo y cierra la pipe; llamar al final de coordinator_run */
static void shutdown_signal_thread(void)
{
    /* cerrar escritura: hace que read() devuelva 0 en el hilo y termine */
    if (sigchld_pipe[1] != -1)
        close(sigchld_pipe[1]);
    sigchld_pipe[1] = -1;

    if (sigchld_thread_running)
    {
        (void)pthread_join(sigchld_thread, NULL);
    }

    if (sigchld_pipe[0] != -1)
        close(sigchld_pipe[0]);
    sigchld_pipe[0] = -1;
}

/* =========================
 *    COORDINATOR RUN
 * ========================= */
void coordinator_run(int total, const char *csvpath)
{
    /* Asegura punto decimal (.) en floats del CSV */
    setlocale(LC_NUMERIC, "C");

    /* Instalar manejo de señales + hilo SIGCHLD (tolerancia a fallos) */
    install_signal_handlers_and_thread();

    /* Abre CSV (csv.c se encarga del encabezado si 'with_header'==1) */
    FILE *f = abrir_csv(csvpath, 1);
    if (!f)
        matar("[COORD] No pude abrir CSV: %s", csvpath);

    const int SLICE_MS = 200; /* espera breve para no bloquear y refrescar logs */
    uint32_t escrito = 0;     /* cantidad de registros escritos */

    fprintf(stdout, "[COORD] escribiendo en '%s' (total=%d)\n", csvpath, total);
    fflush(stdout);
    while (!g_stop && escrito < (uint32_t)total)
    {
        registro_t r;
        int rc = pop_timeout(&r, SLICE_MS); /* intenta sacar del ring con timeout */

        if (rc == 0)
        {
            // Registro disponible: persistir y loguear
            escribir_csv(f, &r);
            ++escrito;
            fprintf(stdout, "[COORD] CSV <- ID=%u (gen=%d, pid=%d) [%u/%d]\n",
                    r.id, r.generador, (int)r.pid, escrito, total);
            fflush(stdout);
        }
        else if (rc == 1)
        {
            // Timeout: revisar estado general
            uint32_t restantes = ipc_restantes();
            int vivos = ipc_prods_vivos();

            // Caso borde: si no quedan productores vivos
            if (vivos == 0)
            {
                fprintf(stdout,
                        "[COORD] ⚠ No quedan generadores vivos. Finalizando anticipadamente (%u/%d).\n",
                        escrito, total);
                fflush(stdout);

                // Limpieza de IPCs
                cerrar_csv(f);
                ipc_cerrar_todos(1);
                return; // salimos sin error
            }

            // Caso normal: quedan IDs y productores vivos
            if (restantes == 0)
            {
                // todos los IDs fueron asignados, pero aún hay registros en tránsito
                continue;
            }
        }
        else
        {
            perror("[COORD] pop_timeout");
            break;
        }
    }

    fprintf(stdout, "[COORD] finalizado (%u/%d). CSV listo.\n", escrito, total);
    fflush(stdout);

    /* Cierre del CSV (flush y close) */
    cerrar_csv(f);

    /* Apagar hilo/pipe de SIGCHLD */
    shutdown_signal_thread();
}

