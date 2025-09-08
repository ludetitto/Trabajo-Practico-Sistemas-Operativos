// Punto de entrada: crea IPC, forkea N hijos (generadores), corre coordinador,
// maneja señales, evita zombies y limpia todos los recursos. Con logs.

#include "../include/common.h"
#include "../include/ipc.h"
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

void generator_loop(int idx_generador);
void coordinator_run(int total, const char *csvpath);

static pid_t *g_pids = NULL;
static int g_nprods = 0;
static volatile sig_atomic_t g_stop = 0;

static void usage(const char *p)
{
    fprintf(stderr,
            "Uso: %s -n <generadores> -t <total_registros> -o <salida.csv>\n"
            "Ej:  %s -n 4 -t 10000 -o productos.csv\n",
            p, p);
}

static void kill_children(int sig)
{
    if (!g_pids)
        return;
    for (int i = 0; i < g_nprods; ++i)
    {
        if (g_pids[i] > 0)
            kill(g_pids[i], sig);
    }
}

static void on_term(int s)
{
    (void)s;
    g_stop = 1;
    fprintf(stdout, "[MAIN] señal de terminación recibida. Deteniendo hijos...\n");
    fflush(stdout);
    kill_children(SIGTERM);
}

static void on_chld(int s)
{
    (void)s;
    for (;;)
    {
        int status = 0;
        pid_t pid = waitpid(-1, &status, WNOHANG);
        if (pid <= 0)
            break;
        fprintf(stdout, "[MAIN] hijo pid=%d terminó (status=%d)\n", (int)pid, status);
        fflush(stdout);
        ipc_mark_dead(pid);                // marca muerto para RR
        for (int i = 0; i < g_nprods; ++i) // marca local
            if (g_pids[i] == pid)
                g_pids[i] = 0;
    }
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

    // Señales del padre
    struct sigaction sa = {0};
    sa.sa_handler = on_term;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);

    struct sigaction sc = {0};
    sc.sa_handler = on_chld;
    sigemptyset(&sc.sa_mask);
    sc.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sc, NULL);

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
        else
        {
            g_pids[i] = pid;
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
    kill_children(SIGTERM);

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
    free(g_pids);
    return 0;
}
