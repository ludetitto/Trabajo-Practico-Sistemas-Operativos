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

    // Abre CSV (csv.c se encarga del encabezado si 'with_header'==1)
    FILE *f = abrir_csv(csvpath, 1);
    if (!f)
        matar("[COORD] No pude abrir CSV: %s", csvpath);

    const int SLICE_MS = 200; // espera breve para no bloquear y refrescar logs
    uint32_t escrito = 0;     // cantidad de registros escritos

    fprintf(stdout, "[COORD] escribiendo en '%s' (total=%d)\n", csvpath, total);
    fflush(stdout);

    // Bucle principal: consumir hasta completar 'total' o recibir señal
    while (!g_stop && escrito < (uint32_t)total)
    {
        registro_t r;
        int rc = pop_timeout(&r, SLICE_MS); // intenta sacar del ring con timeout

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
            // Timeout (no llegó nada en el slice): preguntamos si quedan IDs
            // por asignar (ipc_restantes). Si no quedan, pero todavía hay
            // generadores vivos que podrían estar empujando sus registros
            // pendientes, no finalizamos inmediatamente.
            if (ipc_restantes() == 0)
            {
                int vivos = ipc_prods_vivos();
                int nprods = ipc_nprods();
                // Si quedaron generadores vivos pero hay menos vivos que los
                // publicados originalmente, alguno murió: el total esperado
                // ya no será alcanzable. En ese caso finalizamos.
                if (vivos == 0)
                {
                    fprintf(stdout, "[COORD] no quedan generadores. finalizando (%u/%d).\n", escrito, total);
                    fflush(stdout);
                    g_stop = 1;
                    break;
                }
                else if (vivos < nprods)
                {
                    // Si la diferencia se debe a muertes prematuras, cerramos
                    // porque el total ya no será alcanzable. Si no (los hijos
                    // terminaron normalmente), no cerramos aquí.
                    if (ipc_hubo_muerte_prematura())
                    {
                        fprintf(stdout, "[COORD] detectados %d/%d generadores vivos; alguno murió. no se alcanzará el total. finalizando (%u/%d).\n",
                                vivos, nprods, escrito, total);
                        fflush(stdout);
                        g_stop = 1;
                        break;
                    }
                    else
                    {
                        fprintf(stdout, "[COORD] detectados %d/%d generadores vivos; esperando finalización normal...\n",
                                vivos, nprods);
                        fflush(stdout);
                    }
                }
                else
                {
                    fprintf(stdout, "[COORD] no quedan IDs por asignar, pero quedan %d generadores vivos. esperando...\n", vivos);
                    fflush(stdout);
                }
            }
        }
        else
        {
            // Error en semáforo o reloj
            perror("[COORD] pop_timeout");
            break;
        }
    }

    fprintf(stdout, "[COORD] finalizado (%u/%d). CSV listo.\n", escrito, total);
    fflush(stdout);

    // Cierre del CSV (flush y close)
    cerrar_csv(f);
    // Aseguramos liberar/destruir IPCs si el coordinador termina
    fprintf(stdout, "[COORD] limpiando IPCs y saliendo.\n");
    fflush(stdout);
    ipc_cerrar_todos(1);
}
