#include "../include/common.h"
#include "../include/ipc.h"
#include "../include/csv.h"

static volatile sig_atomic_t parar = 0; // traducción: "detener"
static void on_sig(int s) { 
    (void)s; parar = 1; 
} // Handler de señales

static void comando_coordinador(const char* prog){
    fprintf(stderr,
      "Uso: %s -n <generadores> -t <total_registros> -f <csv>\n", prog); // Help command
}
// El coordinador recibe como argumentos:
// -n <generadores>: cantidad de procesos generadores que se van a usar (no se usa en este programa, pero se pasa a los generadores)
// -t <total_registros>: cantidad total de registros a generar (entre todos los generadores)

int main(int argc, char** argv){ // Recibe como argumentos: -n <generadores> -t <total_registros> -f <csv>
    int cant_gen = -1, total = -1;
    const char* csvpath = NULL;
    FILE* f;
    uint32_t escrito = 0;

    for (int i=1;i<argc;i++){ // Parseo de argumentos
        if (!strcmp(argv[i],"-n") && i+1<argc) 
            cant_gen = atoi(argv[++i]);
        else if (!strcmp(argv[i],"-t") && i+1<argc) 
            total = atoi(argv[++i]);
        else if (!strcmp(argv[i],"-f") && i+1<argc) 
            csvpath = argv[++i];
        else if (!strcmp(argv[i],"--help")) { 
            comando_coordinador(argv[0]); return 0; 
        }
    }
    if (cant_gen <= 0 || total <= 0 || !csvpath) { 
        comando_coordinador(argv[0]); return 1; 
    } // Validación de argumentos

    signal(SIGINT,on_sig); signal(SIGTERM,on_sig); // Manejo de señales. 
                                                   // Sirve para que al presionar Ctrl+C se cierre correctamente
    // Funciona como un "coordinador", abre los IPCs y el archivo CSV
    // y va leyendo del buffer circular e imprimiendo en el CSV hasta que
    // se hayan escrito "total" registros o se reciba una señal de terminación

    if (ipc_abrir_todos(1, (uint32_t)total) < 0) matar("ipc_abrir_todos(crear) fallo"); // Abre todos los IPCs, creando si es necesario
    // El segundo parámetro es la cantidad total de IDs que se van a pedir
    // (sirve para inicializar el estado de los IDs)
    // Si ya estaban creados, no los toca
    // Si no, los crea y los inicializa
    // Si falla, termina el programa

    f = abrir_csv(csvpath, 1);
    if (!f) { 
        ipc_cerrar_todos(1); matar("No pude abrir CSV: %s", csvpath); 
    }

    while (!parar && escrito < (uint32_t)total && !(ids_estado->restantes == 0 && cola->cant_elem_ocupados == 0)) { // mientras no se reciba señal y no se hayan escrito todos los registros
        registro_t r;
        if (pop(&r) == 0) { // hay un registro para consumir
            // lo escribo en el CSV
            escribir_csv(f, &r);
            escrito++;
        } else {
            // nada para consumir, dormir un poquito
            struct timespec ts = {.tv_sec=0, .tv_nsec=1000000};
            nanosleep(&ts, NULL);
        }
    }

    cerrar_csv(f);
    ipc_cerrar_todos(1); // liberar recursos
    return 0;
}
