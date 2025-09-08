#include "../include/common.h"
#include "../include/ipc.h"
#include "../include/csv.h"

static volatile sig_atomic_t parar = 0; // traducción: "detener"
static void on_sig(int s)
{
    (void)s;
    parar = 1;
} // Handler de señales

static void comando_coordinador(const char *prog)
{
    fprintf(stderr,
            "Uso: %s -n <generadores> -t <total_registros> -f <csv>\n", prog); // Help command
}

int main(int argc, char **argv)
{ // Recibe como argumentos: -n <generadores> -t <total_registros> -f <csv>
    int cant_gen = -1, total = -1, salir = 0;
    const char *csvpath = NULL;
    FILE *f;
    uint32_t escrito = 0;
    const int TIMEOUT_MS = 10000;

    for (int i = 1; i < argc; i++)
    { // Parseo de argumentos
        if (!strcmp(argv[i], "-n") && i + 1 < argc)
            cant_gen = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-t") && i + 1 < argc)
            total = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-f") && i + 1 < argc)
            csvpath = argv[++i];
        else if (!strcmp(argv[i], "--help"))
        {
            comando_coordinador(argv[0]);
            return 0;
        }
    }
    if (cant_gen <= 0 || total <= 0 || !csvpath)
    {
        comando_coordinador(argv[0]);
        return 1;
    } // Validación de argumentos

    signal(SIGINT, on_sig);
    signal(SIGTERM, on_sig); // Manejo de señales.

    if (ipc_abrir_todos(1, (uint32_t)total) < 0)
        matar("ipc_abrir_todos(crear) falló al ejecutarse.");

    f = abrir_csv(csvpath, 1);
    if (!f)
    {
        ipc_cerrar_todos(1);
        matar("No pude abrir CSV: %s", csvpath);
    }

    while (!parar && escrito < (uint32_t)total && !salir)
    {
        registro_t r;
        int rc = pop_timeout(&r, TIMEOUT_MS);

        if (!rc)
        {
            // llegó un registro
            escribir_csv(f, &r);
            // LOG del coordinador: muestra id y qué generador lo produjo
            printf("[COORD] escrito ID=%u (gen=%d, pid=%d)\n", r.id, r.generador, (int)r.pid);
            fflush(stdout);

            escrito++;
        }
        else if (rc == 1)
        {
            // timeout
            fprintf(stderr, "[coordinador] %d ms sin recibir registros. Finalizo.\n", TIMEOUT_MS);
            salir = 1;
        }
        else
        {
            // error en semáforo
            perror("[coordinador] pop_timeout");
            salir = 1;
        }
    }

    cerrar_csv(f);
    ipc_cerrar_todos(1); // liberar recursos
    return 0;
}
