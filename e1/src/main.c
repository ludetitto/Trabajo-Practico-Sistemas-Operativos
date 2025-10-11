// Punto de entrada: crea IPC, forkea N hijos (generadores), corre coordinador,
// maneja señales, evita zombies y limpia todos los recursos. Con logs.

#include "../include/main.h"
#include <pthread.h>

static pid_t *g_pids = NULL;
static int g_nprods = 0;
static volatile sig_atomic_t g_stop = 0;

// self-pipe para señales (SIGCHLD, SIGTERM, SIGINT, SIGHUP)
static int sigpipe_fd[2] = {-1, -1};

static void matar_hijos(int sig);

// Signal handler: escribe la señal en el pipe
static void sig_a_pipe(int s)
{
    char c = (s == SIGCHLD) ? 'C' : 'T';
    if (sigpipe_fd[1] != -1)
    {
        ssize_t r = write(sigpipe_fd[1], &c, 1);
        (void)r; // ignore errors (can't do much in handler)
    }
}

static void usage(const char *p)
{
    fprintf(stderr,
            "Uso: %s -n <generadores> -t <total_registros> -o <salida.csv>\n"
            "Ej:  %s -n 4 -t 10000 -o productos.csv\n",
            p, p);
}

static void matar_hijos(int sig)
{
    if (!g_pids)
        return;
    for (int i = 0; i < g_nprods; ++i)
    {
        if (g_pids[i] > 0)
            kill(g_pids[i], sig);
    }
}

// thread: lee el pipe de señales y maneja SIGCHLD y señales de terminación
static void *sig_thread_lector(void *arg)
{
    (void)arg;
    char buf[64];
    while (1)
    {
        ssize_t n = read(sigpipe_fd[0], buf, sizeof(buf));
        if (n == 0)
        {
            break; // Pipe cerrado -> salir
        }
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                struct timespec ts = {.tv_sec = 0, .tv_nsec = 10000000L}; // 10ms
                nanosleep(&ts, NULL);
                continue;
            }
            break;
        }

        for (ssize_t i = 0; i < n; ++i)
        {
            char c = buf[i];
            if (c == 'C')
            {
                for (;;)
                {
                    int status = 0;
                    pid_t pid = waitpid(-1, &status, WNOHANG);
                    if (pid <= 0)
                        break;
                    fprintf(stdout, "[MAIN] hijo pid=%d terminó (status=%d)\n", (int)pid, status);
                    fflush(stdout);
                    ipc_mark_dead(pid); // seguro fuera del handler
                    for (int j = 0; j < g_nprods; ++j)
                        if (g_pids[j] == pid)
                            g_pids[j] = 0;
                }
            }
            else if (c == 'T')
            {
                g_stop = 1;
                fprintf(stdout, "[MAIN] señal de terminación recibida (pipe). Deteniendo hijos...\n");
                fflush(stdout);
                matar_hijos(SIGTERM);
            }
        }
    }
    return NULL;
}

int main(int argc, char **argv)
{
    int nprods = -1, total = -1;
    const char *out_csv = NULL;

    int opt;
    while ((opt = getopt(argc, argv, "n:t:o:h")) != -1)
    {
        switch (opt)
        {
        case 'n':
            nprods = atoi(optarg);
            break;
        case 't':
            total = atoi(optarg);
            break;
        case 'o':
            out_csv = optarg;
            break;
        case 'h':
        default:
            usage(argv[0]);
            return (opt == 'h') ? 0 : 1;
        }
    }
    if (nprods <= 0 || nprods > MAX_PRODS || total <= 0 || !out_csv)
    {
        usage(argv[0]);
        return 1;
    }

    // Self-pipe para manejar señales en thread separado
    if (pipe(sigpipe_fd) == -1)
        matar("[PIPE] pipe() falló");
        fprintf(stdout, "[MAIN] pipe creado: readfd=%d writefd=%d\n", sigpipe_fd[0], sigpipe_fd[1]);
        fflush(stdout);

    // Escribe en el pipe de forma no bloqueante
    int flags = fcntl(sigpipe_fd[1], F_GETFL, 0);
    fcntl(sigpipe_fd[1], F_SETFL, flags | O_NONBLOCK);

    // thread lector del pipe
    pthread_t th;
    if (pthread_create(&th, NULL, sig_thread_lector, NULL) != 0)
        matar("[PIPE] pthread_create falló");

    // Señales del padre
    struct sigaction sa = {0};
    sa.sa_handler = sig_a_pipe;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGCHLD, &sa, NULL);

    fprintf(stdout, "[MAIN] creando IPC (total=%d)...\n", total);
    fflush(stdout);
    if (ipc_abrir_todos(1, (uint32_t)total) < 0)
    {
        matar("ipc_abrir_todos(crear) falló.");
    }

    g_nprods = nprods;
    g_pids = calloc((size_t)nprods, sizeof(pid_t));
    if (!g_pids)
        matar("calloc pids");

    // Fork de N hijos
    fprintf(stdout, "[MAIN] forkeando %d generadores...\n", nprods);
    fflush(stdout);
    for (int i = 0; i < nprods; ++i)
    {
        pid_t pid = fork();
        if (pid < 0)
        {
            perror("fork");
            matar("No se pudo crear el generador %d", i);
        }
        if (pid == 0)
        {
            generator_loop(i); // no vuelve
        }
            fprintf(stdout, "[MAIN] sig pipe: leidos %zd bytes\n", n);
        else
        {
            g_pids[i] = pid;
                fprintf(stdout, "[MAIN] sig pipe: byte[%zd] = '%c'\n", i, (c >= 32 && c < 127) ? c : '?');
                fflush(stdout);
            fprintf(stdout, "[MAIN] generador idx=%d pid=%d listo.\n", i, (int)pid);
            fflush(stdout);
        }
    }

    // Publica metadata de hijos (para RR estricto)
    ipc_set_children(nprods, g_pids);

    // Coordinador (loop interruptible)
    fprintf(stdout, "[MAIN] iniciando coordinador → %s\n", out_csv);
    fflush(stdout);
    coordinator_run(total, out_csv);

    // Si salimos por señal, aseguramos apagar hijos
    matar_hijos(SIGTERM);

    // Reap final (sin zombies)
    fprintf(stdout, "[MAIN] esperando hijos...\n");
    fflush(stdout);
    for (;;)
    {
        int status = 0;
        pid_t pid = waitpid(-1, &status, 0);
        if (pid == -1 && errno == ECHILD)
            break;
        if (pid <= 0)
            break;
        fprintf(stdout, "[MAIN] (reap) pid=%d status=%d\n", (int)pid, status);
        fflush(stdout);
    }

    fprintf(stdout, "[MAIN] limpiando IPC y cerrando.\n");
    fflush(stdout);
    ipc_cerrar_todos(1);
    // Cerrar pipe, thread y liberar pids
    if (sigpipe_fd[0] != -1)
        close(sigpipe_fd[0]);
    if (sigpipe_fd[1] != -1)
        close(sigpipe_fd[1]);
    pthread_cancel(th);
    pthread_join(th, NULL);
    free(g_pids);
    return 0;
}
