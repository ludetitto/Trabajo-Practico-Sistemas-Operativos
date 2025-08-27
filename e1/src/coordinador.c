#include "../include/common.h"
#include "../include/ipc.h"
#include "../include/csv.h"

static volatile sig_atomic_t stop = 0; // Variable GLOBAL para manejar señales
static void on_sig(int s){ (void)s; stop = 1; } // Handler de señales

static void usage(const char* prog){
    fprintf(stderr,
      "Uso: %s -n <generadores> -t <total_registros> -f <csv>\n", prog); // Help command
}

int main(int argc, char** argv){ // Recibe como argumentos: -n <generadores> -t <total_registros> -f <csv>
    int N = -1; int total = -1; const char* csvpath = NULL;

    for (int i=1;i<argc;i++){ // Parseo de argumentos
        if (!strcmp(argv[i],"-n") && i+1<argc) N = atoi(argv[++i]);
        else if (!strcmp(argv[i],"-t") && i+1<argc) total = atoi(argv[++i]);
        else if (!strcmp(argv[i],"-f") && i+1<argc) csvpath = argv[++i];
        else if (!strcmp(argv[i],"--help")) { usage(argv[0]); return 0; }
    }
    if (N<=0 || total<=0 || !csvpath){ usage(argv[0]); return 1; } // Validación de argumentos

    signal(SIGINT,on_sig); signal(SIGTERM,on_sig); // Manejo de señales. 
                                                   // Sirve para que al presionar Ctrl+C se cierre correctamente
    // Funciona como un "coordinador", abre los IPCs y el archivo CSV
    // y va leyendo del buffer circular e imprimiendo en el CSV hasta que
    // se hayan escrito "total" registros o se reciba una señal de terminación

    if (ipc_open_all(1, (uint32_t)total) < 0) die("ipc_open_all(create) fallo"); // Abre todos los IPCs, creando si es necesario
    // El segundo parámetro es la cantidad total de IDs que se van a pedir
    // (sirve para inicializar el estado de los IDs)
    // Si ya estaban creados, no los toca
    // Si no, los crea y los inicializa
    // Si falla, termina el programa

    FILE* f = csv_open(csvpath, 1);
    if (!f) { ipc_close_all(1); die("No pude abrir CSV: %s", csvpath); }

    uint32_t written = 0;
    while (!stop && written < (uint32_t)total) { // mientras no se reciba señal y no se hayan escrito todos los registros
        record_t r;
        if (ring_pop(&r)==0) { // hay un registro para consumir
            // lo escribo en el CSV
            csv_write(f, &r);
            written++;
        } else {
            // nada para consumir, dormir un poquito
            if (g_ids->remaining == 0 && g_ring->count == 0) {
                break; // ya no quedan IDs y el buffer está vacío, puedo terminar
            }
            struct timespec ts = {.tv_sec=0, .tv_nsec=1000000};
            nanosleep(&ts, NULL);
        }
    }

    csv_close(f);
    ipc_close_all(1); // liberar recursos
    return 0;
}
