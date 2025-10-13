// Generador: pide bloques de 10 IDs en RR estricto y produce 1 registro por ID.
// Agrega logs en consola y un delay aleatorio de 100–1000 ms entre registros.

#include "../include/generador.h"

static volatile sig_atomic_t g_stop = 0;

static void on_term(int s)
{
    (void)s;
    g_stop = 1;
}

static inline void sleep_ms(int ms)
{
    if (ms <= 0)
        return;
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR)
    {
    }
}

void generator_loop(int idx_generador)
{
#ifdef PR_SET_PDEATHSIG
    prctl(PR_SET_PDEATHSIG, SIGTERM);
#endif

    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    struct sigaction sa = {0};
    sa.sa_handler = on_term;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);

    if (ipc_abrir_todos(0, 0) < 0)
    {
        matar("[GEN %d] ipc_abrir_todos(abrir) falló.", idx_generador);
    }

    srand((unsigned)(getpid() ^ (unsigned)time(NULL)));
    fprintf(stdout, "[GEN %d][pid=%d] iniciado.\n", idx_generador, (int)getpid());
    fflush(stdout);

    while (!g_stop)
    {
        uint32_t base = 0, cont = 0;
        int sin_ids = pedir_bloque_ids_rr(idx_generador, &base, &cont);
        if (sin_ids)
        {
            fprintf(stdout, "[GEN %d][pid=%d] no hay más IDs. Fin.\n",
                    idx_generador, (int)getpid());
            fflush(stdout);
            break;
        }

        uint32_t ultimo = base + cont - 1;
        fprintf(stdout, "[GEN %d][pid=%d] bloque asignado: %u..%u (%u IDs)\n",
                idx_generador, (int)getpid(), base, ultimo, cont);
        fflush(stdout);

        for (uint32_t i = 0; i < cont && !g_stop; ++i)
        {
            registro_t r;
            generar_randrec(&r, base + i, idx_generador);
            r.pid = getpid();

            int d_ms = 100 + (rand() % 901);
            fprintf(stdout, "[GEN %d][pid=%d] ID=%u → push (delay=%d ms)\n",
                    idx_generador, (int)r.pid, r.id, d_ms);
            fflush(stdout);
            sleep_ms(d_ms);

            int pushed = 0;
            for (;;)
            {
                if (g_stop)
                    break;
                int prc = push_interruptible(&r, 200);
                if (prc == 0)
                {
                    pushed = 1;
                    break;
                }
                if (prc == 1)
                    continue; // timeout
                perror("[GEN] push_interruptible");
                g_stop = 1;
                break;
            }
            if (pushed)
            {
                // === IMPORTANTE: avisar que produjimos 1 ID del bloque vigente ===
                ipc_avance_bloque(idx_generador);
            }
        }
    }

    fprintf(stdout, "[GEN %d][pid=%d] saliendo.\n", idx_generador, (int)getpid());
    fflush(stdout);

    ipc_cerrar_todos(0);
    _exit(0);
}
