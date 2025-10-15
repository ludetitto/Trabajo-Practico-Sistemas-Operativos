// main.c — Administrador (abuelo) + Coordinador (hijo que forkea generadores)
//
// - Proceso "Administrador" (padre): parsea args, fork() del Coordinador,
//   maneja señales (SIGINT/SIGTERM/SIGHUP, SIGCHLD del Coordinador), y
//   si el Coordinador muere, intenta limpieza de respaldo (IPC).
//
// - Proceso "Coordinador" (hijo): crea IPC, forkea N generadores, publica
//   pids (ipc_set_children), ejecuta coordinator_run(), apaga generadores
//   y limpia IPCs.
//
// Nota: No se modifican coordinador.c, generador.c, ipc.c, csv.c ni randrec.c.
//       coordinator_run() ya maneja SIGCHLD de generadores y su lógica propia.

#include "../include/main.h"
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// ====== Prototipos locales ======
static void usage(const char *p);
static int  child_main(int nprods, int total, const char *out_csv); // Coordinador
static void administrador_loop(pid_t child_pid, const char *out_csv);

// ====== Helpers del Coordinador (HIJO) ======
static pid_t *g_pids = NULL;
static int    g_nprods = 0;

static void matar_hijos(int sig)
{
    if (!g_pids) return;
    for (int i = 0; i < g_nprods; ++i)
        if (g_pids[i] > 0)
            kill(g_pids[i], sig);
}

static void set_line_buffers(void)
{
    setvbuf(stdout, NULL, _IOLBF, 0); // stdout line-buffered
    setvbuf(stderr, NULL, _IONBF, 0); // stderr unbuffered
}

// =============================
//        COORDINADOR (HIJO)
// =============================
static int child_main(int nprods, int total, const char *out_csv)
{
    set_line_buffers();
    fprintf(stdout, "[Coordinador] inicio (pid=%d) — n=%d, total=%d, csv=%s\n",
            (int)getpid(), nprods, total, out_csv);
    fflush(stdout);

    // Crea todos los IPCs
    if (ipc_abrir_todos(1, (uint32_t)total) < 0) {
        matar("[Coordinador] ipc_abrir_todos(crear) falló.");
    }

    // Fork de N generadores (el Coordinador es su padre directo)
    g_nprods = nprods;
    g_pids = calloc((size_t)nprods, sizeof(pid_t));
    if (!g_pids) matar("[Coordinador] calloc pids");

    fprintf(stdout, "[Coordinador] forkeando %d generadores…\n", nprods);
    fflush(stdout);

    for (int i = 0; i < nprods; ++i) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            matar("[Coordinador] No se pudo crear el generador %d", i);
        }
        if (pid == 0) {
            // Proceso Generador (no vuelve)
            generator_loop(i);
            _exit(0);
        } else {
            g_pids[i] = pid;
            fprintf(stdout, "[Coordinador] generador idx=%d pid=%d listo.\n", i, (int)pid);
            fflush(stdout);
        }
    }

    // Publica tabla de hijos para RR
    ipc_set_children(nprods, g_pids);

    // Ejecuta el bucle del coordinador (consume el ring y escribe CSV)
    fprintf(stdout, "[Coordinador] iniciando coordinator_run() → %s\n", out_csv);
    fflush(stdout);
    coordinator_run(total, out_csv);

    // Si salimos por aquí, apagamos generadores por las dudas
    matar_hijos(SIGTERM);

    // Reap final (evita zombies si quedara alguno)
    fprintf(stdout, "[Coordinador] esperando generadores…\n");
    fflush(stdout);
    for (;;) {
        int status = 0;
        pid_t pid = waitpid(-1, &status, 0);
        if (pid == -1 && errno == ECHILD) break;
        if (pid <= 0) break;
        fprintf(stdout, "[Coordinador] (reap) pid=%d status=%d\n", (int)pid, status);
        fflush(stdout);
    }

    // Limpieza final de IPC (coordinator_run ya llamó ipc_cerrar_todos(1),
    // pero si llegamos acá por otra ruta, garantizamos):
    fprintf(stdout, "[Coordinador] limpieza final de IPC.\n");
    fflush(stdout);
    ipc_cerrar_todos(1);

    free(g_pids);
    fprintf(stdout, "[Coordinador] fin OK.\n");
    fflush(stdout);
    return 0;
}

// ==================================
//        ADMINISTRADOR (ABUELO)
// ==================================
static volatile sig_atomic_t g_admin_stop = 0;
static pid_t g_child_pid = -1;

static void admin_on_term(int s)
{
    (void)s;
    g_admin_stop = 1;
    if (g_child_pid > 0) {
        // Pide terminación ordenada del Coordinador
        kill(g_child_pid, SIGTERM);
    }
}

static void usage(const char *p)
{
    fprintf(stderr,
            "Uso: %s -n <generadores> -t <total_registros> -o <salida.csv>\n"
            "Ej:  %s -n 4 -t 10000 -o productos.csv\n",
            p, p);
}

// El administrador espera al Coordinador; si éste muere, intenta limpieza de respaldo.
static void administrador_loop(pid_t child_pid, const char *out_csv)
{
    set_line_buffers();

    // Handlers del Administrador
    struct sigaction sa = {0};
    sa.sa_handler = admin_on_term;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP,  &sa, NULL);

    g_child_pid = child_pid;

    fprintf(stdout, "[Administrador] hijo Coordinador pid=%d, CSV=%s\n",
            (int)child_pid, out_csv ? out_csv : "(null)");
    fflush(stdout);

    // Espera al Coordinador
    int status = 0;
    for (;;) {
        pid_t w = waitpid(child_pid, &status, 0);
        if (w == -1) {
            if (errno == EINTR) {
                // Señal recibida: ya mandamos SIGTERM en admin_on_term()
                // Si sigue vivo, damos un pequeño margen y escalamos a SIGKILL
                if (kill(child_pid, 0) == 0) {
                    struct timespec ts = {.tv_sec=0, .tv_nsec=200000000L}; // 200ms
                    nanosleep(&ts, NULL);
                    if (kill(child_pid, 0) == 0) {
                        fprintf(stderr, "[Administrador] escalando a SIGKILL sobre hijo %d\n",
                                (int)child_pid);
                        kill(child_pid, SIGKILL);
                    }
                }
                continue;
            }
            perror("[Administrador] waitpid");
            break;
        } else {
            // Coordinador terminó
            fprintf(stdout, "[Administrador] Coordinador %d terminó (status=%d)\n",
                    (int)w, status);
            fflush(stdout);
            break;
        }
    }

    // Limpieza de respaldo de IPCs (por si el Coordinador murió abruptamente)
    // Intentamos abrir IPCs existentes (crear=0); si abre, los cerramos/desvinculamos.
    if (ipc_abrir_todos(0, 0) == 0) {
        fprintf(stdout, "[Administrador] limpieza de respaldo de IPCs…\n");
        fflush(stdout);
        ipc_cerrar_todos(1);
    }

    fprintf(stdout, "[Administrador] fin.\n");
    fflush(stdout);
}

// ============================
//             main
// ============================
int main(int argc, char **argv)
{
    set_line_buffers();

    int nprods = -1, total = -1;
    const char *out_csv = NULL;

    int opt;
    while ((opt = getopt(argc, argv, "n:t:o:h")) != -1) {
        switch (opt) {
        case 'n': nprods = atoi(optarg); break;
        case 't': total  = atoi(optarg); break;
        case 'o': out_csv = optarg;      break;
        case 'h':
        default:
            usage(argv[0]);
            return (opt == 'h') ? 0 : 1;
        }
    }
    if (nprods <= 0 || nprods > MAX_PRODS || total <= 0 || !out_csv) {
        usage(argv[0]);
        return 1;
    }

    // ADMINISTRADOR forkea al COORDINADOR
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 2;
    }

    if (pid == 0) {
        // ======= Proceso HIJO: COORDINADOR =======
        // (Crea IPCs, forkea generadores y ejecuta coordinator_run)
        int rc = child_main(nprods, total, out_csv);
        _exit(rc == 0 ? 0 : 3);
    } else {
        // ======= Proceso PADRE: ADMINISTRADOR =======
        administrador_loop(pid, out_csv);
        return 0;
    }
}
