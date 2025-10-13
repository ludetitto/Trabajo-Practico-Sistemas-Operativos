// main.c — Supervisor (abuelo) + Hijo coordinador con lógica original
// - Supervisor (proceso padre): parsea args, fork() del hijo, maneja señales globales,
//   apaga al hijo ordenadamente y garantiza ipc_cerrar_todos(1) al final.
// - Hijo (coordinador): crea IPC, forkea N generadores, corre coordinator_run(),
//   maneja SIGCHLD/SIGTERM/SIGINT/SIGHUP como antes, y limpia recursos.

#include "../include/main.h"
#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// ===================== Prototipos locales =====================
static void usage(const char *p);
static void supervisor_loop(pid_t child_pid);
static int child_main(int nprods, int total, const char *out_csv);

// ======== Sección: lógica del HIJO (coordinador + generadores) ========
// Reuso de las globals y helpers de tu main original:
static pid_t *g_pids = NULL;
static int g_nprods = 0;
static volatile sig_atomic_t g_stop = 0;

// self-pipe para señales (SIGCHLD, SIGTERM, SIGINT, SIGHUP)
static int sigpipe_fd[2] = {-1, -1};

static void matar_hijos(int sig)
{
    if (!g_pids)
        return;
    for (int i = 0; i < g_nprods; ++i)
        if (g_pids[i] > 0)
            kill(g_pids[i], sig);
}

// Signal handler (HIJO): escribe la señal en el pipe
static void sig_a_pipe(int s)
{
    char c = (s == SIGCHLD) ? 'C' : 'T';
    if (sigpipe_fd[1] != -1)
    {
        ssize_t r = write(sigpipe_fd[1], &c, 1);
        (void)r;
    }
}

// thread lector del pipe (HIJO)
static void *sig_thread_lector(void *arg)
{
    (void)arg;
    char buf[64];
    while (1)
    {
        ssize_t n = read(sigpipe_fd[0], buf, sizeof(buf));
        if (n == 0)
            break; // Pipe cerrado → salir
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
                    fprintf(stdout, "[CHILD] hijo pid=%d terminó (status=%d)\n", (int)pid, status);
                    fflush(stdout);
                    ipc_mark_dead(pid);
                    for (int j = 0; j < g_nprods; ++j)
                        if (g_pids[j] == pid)
                            g_pids[j] = 0;
                }
            }
            else if (c == 'T')
            {
                g_stop = 1;
                fprintf(stdout, "[CHILD] señal de terminación recibida. Deteniendo generadores...\n");
                fflush(stdout);
                matar_hijos(SIGTERM);
            }
        }
    }
    return NULL;
}

static int child_main(int nprods, int total, const char *out_csv)
{
    // Buffers del HIJO (coordinador): stdout por línea, stderr sin buffer
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    // Self-pipe del hijo
    if (pipe(sigpipe_fd) == -1)
    {
        matar("[CHILD PIPE] pipe() falló");
    }
    fprintf(stdout, "[CHILD] pipe creado: readfd=%d writefd=%d\n", sigpipe_fd[0], sigpipe_fd[1]);
    fflush(stdout);

    int flags = fcntl(sigpipe_fd[1], F_GETFL, 0);
    fcntl(sigpipe_fd[1], F_SETFL, flags | O_NONBLOCK);

    pthread_t th;
    if (pthread_create(&th, NULL, sig_thread_lector, NULL) != 0)
        matar("[CHILD PIPE] pthread_create falló");

    // Señales del HIJO (coordinador)
    struct sigaction sa = {0};
    sa.sa_handler = sig_a_pipe;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGCHLD, &sa, NULL);

    fprintf(stdout, "[CHILD] creando IPC (total=%d)...\n", total);
    fflush(stdout);
    if (ipc_abrir_todos(1, (uint32_t)total) < 0)
    {
        matar("[CHILD] ipc_abrir_todos(crear) falló.");
    }

    g_nprods = nprods;
    g_pids = calloc((size_t)nprods, sizeof(pid_t));
    if (!g_pids)
        matar("[CHILD] calloc pids");

    // Fork de N generadores
    fprintf(stdout, "[CHILD] forkeando %d generadores...\n", nprods);
    fflush(stdout);
    for (int i = 0; i < nprods; ++i)
    {
        pid_t pid = fork();
        if (pid < 0)
        {
            perror("fork");
            matar("[CHILD] No se pudo crear el generador %d", i);
        }
        if (pid == 0)
        {
            generator_loop(i); // no vuelve
        }
        else
        {
            g_pids[i] = pid;
            fprintf(stdout, "[CHILD] generador idx=%d pid=%d listo.\n", i, (int)pid);
            fflush(stdout);
        }
    }

    // Publica metadata para RR
    ipc_set_children(nprods, g_pids);

    // Coordinador (loop interruptible)
    fprintf(stdout, "[CHILD] iniciando coordinador → %s\n", out_csv);
    fflush(stdout);
    coordinator_run(total, out_csv);

    // Si salimos por señal, aseguramos apagar hijos
    matar_hijos(SIGTERM);

    // Reap final
    fprintf(stdout, "[CHILD] esperando generadores...\n");
    fflush(stdout);
    for (;;)
    {
        int status = 0;
        pid_t pid = waitpid(-1, &status, 0);
        if (pid == -1 && errno == ECHILD)
            break;
        if (pid <= 0)
            break;
        fprintf(stdout, "[CHILD] (reap) pid=%d status=%d\n", (int)pid, status);
        fflush(stdout);
    }

    fprintf(stdout, "[CHILD] limpieza final de IPC.\n");
    fflush(stdout);
    ipc_cerrar_todos(1);

    // Cerrar pipe, thread y liberar — sin pthread_cancel
    g_stop = 1;
    if (sigpipe_fd[1] != -1)
    {
        close(sigpipe_fd[1]);
        sigpipe_fd[1] = -1;
    } // provoca EOF en el hilo
    pthread_join(th, NULL);
    if (sigpipe_fd[0] != -1)
    {
        close(sigpipe_fd[0]);
        sigpipe_fd[0] = -1;
    }

    free(g_pids);
    return 0;
}

// ===================== Sección: SUPERVISOR (abuelo) =====================

static volatile sig_atomic_t g_sup_stop = 0;
static pid_t g_child_pid = -1;

static void sup_on_term(int s)
{
    (void)s;
    g_sup_stop = 1;
    if (g_child_pid > 0)
    {
        // Apaga al hijo ordenadamente
        kill(g_child_pid, SIGTERM);
    }
}

static void usage(const char *p)
{
    fprintf(stderr,
            "Uso: %s -n <generadores> -t <total_registros> -o <salida.csv>\n"
            "Ej:  %s -n 4 -t 100 -o ../productos.csv\n",
            p, p);
}

static void supervisor_loop(pid_t child_pid)
{
    g_child_pid = child_pid;

    // Buffers del SUPERVISOR: stdout por línea, stderr sin buffer
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    // Señales del SUPERVISOR
    struct sigaction sa = {0};
    sa.sa_handler = sup_on_term;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);

    // Espera al hijo; si recibe señal, intenta apagado ordenado y escalado
    int status = 0;
    for (;;)
    {
        pid_t w = waitpid(child_pid, &status, 0);
        if (w == -1)
        {
            if (errno == EINTR)
            {
                // Señal recibida: ya mandamos SIGTERM en sup_on_term()
                // Espera breve y, si sigue vivo, SIGKILL.
                if (kill(child_pid, 0) == 0)
                {
                    struct timespec ts = {.tv_sec = 0, .tv_nsec = 200000000L}; // 200ms
                    nanosleep(&ts, NULL);
                    if (kill(child_pid, 0) == 0)
                    {
                        fprintf(stderr, "[SUP] escalando a SIGKILL sobre hijo %d\n", (int)child_pid);
                        kill(child_pid, SIGKILL);
                    }
                }
                continue;
            }
            perror("[SUP] waitpid");
            break;
        }
        else
        {
            fprintf(stdout, "[SUP] hijo %d terminó (status=%d)\n", (int)w, status);
            fflush(stdout);
            break;
        }
    }

    // Limpieza de respaldo (por si el hijo no alcanzó a hacerlo)
    fprintf(stdout, "[SUP] limpieza final de IPC (respaldo)…\n");
    fflush(stdout);
    ipc_cerrar_todos(1);
}

// ============================ main (SUPERVISOR) ============================

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

    // Buffers del SUPERVISOR antes del fork (por si imprime antes de supervisor_loop)
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    // Fork del HIJO coordinador
    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        return 2;
    }

    if (pid == 0)
    {
        // ======== Proceso HIJO ========
        return child_main(nprods, total, out_csv);
    }
    else
    {
        // ======== Proceso SUPERVISOR (ABUELO) ========
        fprintf(stdout, "[SUP] lanzado hijo coordinador pid=%d\n", (int)pid);
        fflush(stdout);
        supervisor_loop(pid);
        fprintf(stdout, "[SUP] fin.\n");
        fflush(stdout);
        return 0;
    }
}
