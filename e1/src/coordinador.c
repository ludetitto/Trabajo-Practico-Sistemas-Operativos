// Coordinador: escribe encabezado y consume 'total' registros.
// Usa pop_timeout corto para reaccionar a señales/caídas y loguea cada escritura.

#include "../include/coordinador.h"

// Flag global para terminar ordenadamente ante señales
static volatile sig_atomic_t g_stop = 0;

// Handler de señales: marca fin de ejecución
static void on_term(int s)
{
    (void)s;
    g_stop = 1;
}

void coordinator_run(int total, const char *csvpath)
{
    // Asegura punto decimal (.) en floats del CSV
    setlocale(LC_NUMERIC, "C");

    // Instala handlers de terminación
    struct sigaction sa = {0};
    sa.sa_handler = on_term;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);

    // Abre CSV (csv.c escribe encabezado si with_header==1)
    FILE *f = abrir_csv(csvpath, 1);
    if (!f)
        matar("[COORD] No pude abrir CSV: %s", csvpath);

    const int SLICE_MS = 200; // espera breve para refrescar y reaccionar a señales
    uint32_t escrito = 0;

    fprintf(stdout, "[COORD] escribiendo en '%s' (total=%d)\n", csvpath, total);
    fflush(stdout);

    while (!g_stop && escrito < (uint32_t)total)
    {
        registro_t r;
        int rc = pop_timeout(&r, SLICE_MS);

        if (rc == 0)
        {
            // Registro disponible
            escribir_csv(f, &r);
            ++escrito;
            fprintf(stdout, "[COORD] CSV <- ID=%u (gen=%d, pid=%d) [%u/%d]\n",
                    r.id, r.generador, (int)r.pid, escrito, total);
            fflush(stdout);
            continue;
        }

        if (rc == 1)
        {
            // Timeout: evaluar estado del sistema
            int vivos = ipc_prods_vivos();
            uint32_t pendientes = ipc_pendientes_total(); // RESTANTES + DEVOLUCIONES

            // 1) Si no queda ningún generador vivo, nadie más va a producir:
            if (vivos == 0)
            {
                fprintf(stdout, "[COORD] sin generadores vivos; quedan %u IDs pendientes. finalizando (%u/%d).\n",
                        pendientes, escrito, total);
                fflush(stdout);
                g_stop = 1;
                break;
            }

            // 2) Si no quedan pendientes (ni nuevos ni devueltos), terminamos:
            if (pendientes == 0)
            {
                fprintf(stdout, "[COORD] no quedan IDs pendientes. finalizando (%u/%d).\n",
                        escrito, total);
                fflush(stdout);
                g_stop = 1;
                break;
            }

            // 3) Caso intermedio: hay vivos y hay trabajo → seguimos esperando.
            continue;
        }

        // rc == -1 → error real (señal/interrupción o fallo de reloj/semaforo)
        perror("[COORD] pop_timeout");
        break;
    }

    fprintf(stdout, "[COORD] finalizado (%u/%d). CSV listo.\n", escrito, total);
    fflush(stdout);

    // Cierre del CSV (flush y close)
    cerrar_csv(f);

    // Limpieza explícita de IPCs al terminar el coordinador
    fprintf(stdout, "[COORD] limpiando IPCs y saliendo.\n");
    fflush(stdout);
    ipc_cerrar_todos(1);
}
