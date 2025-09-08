// Coordinador: escribe encabezado y consume 'total' registros.
// Usa pop_timeout corto para reaccionar a señales/caídas y loguea cada escritura.

#include "../include/common.h"
#include "../include/ipc.h"
#include "../include/csv.h"
#include <locale.h>
#include <signal.h>

static volatile sig_atomic_t g_stop = 0;
static void on_term(int s)
{
    (void)s;
    g_stop = 1;
}

void coordinator_run(int total, const char *csvpath)
{
    setlocale(LC_NUMERIC, "C");

    struct sigaction sa = {0};
    sa.sa_handler = on_term;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);

    FILE *f = abrir_csv(csvpath, 1);
    if (!f)
        matar("[COORD] No pude abrir CSV: %s", csvpath);

    const int SLICE_MS = 200; // slices cortos para refrescar la consola seguido
    uint32_t escrito = 0;

    fprintf(stdout, "[COORD] escribiendo en '%s' (total=%d)\n", csvpath, total);
    fflush(stdout);

    while (!g_stop && escrito < (uint32_t)total)
    {
        registro_t r;
        int rc = pop_timeout(&r, SLICE_MS);
        if (rc == 0)
        {
            escribir_csv(f, &r);
            ++escrito;
            fprintf(stdout, "[COORD] CSV <- ID=%u (gen=%d, pid=%d) [%u/%d]\n",
                    r.id, r.generador, (int)r.pid, escrito, total);
            fflush(stdout);
        }
        else if (rc == 1)
        {
            // timeout: si no quedan IDs por asignar, seguimos “barriendo” el ring
            if (ipc_restantes() == 0)
            {
                // no hacemos nada; volvemos a intentar hasta completar 'total' o señal
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

    cerrar_csv(f);
}
