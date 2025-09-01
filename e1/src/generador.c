#include "../include/common.h"
#include "../include/ipc.h"
#include "../include/randrec.h"

static void comando_generador(const char* prog){ // Help command
    fprintf(stderr,"Uso: %s -q <cantidad_por_generador> -g <idx>\n", prog);
}

int main(int argc, char** argv){
    int cant_a_producir = -1, // cantidad de registros a generar por este proceso
        idx_generador = 0, // índice del generador (opcional, por defecto 0)
        cant_producida = 0; // cantidad de registros generados hasta ahora

    for (int i = 1; i < argc; i++){ // Parseo de argumentos
        if (!strcmp(argv[i],"-q") && i+1<argc) 
            cant_a_producir = atoi(argv[++i]);
        else if (!strcmp(argv[i],"-g") && i+1<argc) 
            idx_generador = atoi(argv[++i]);
        else if (!strcmp(argv[i],"--help")) { 
            comando_generador(argv[0]); return 0; 
        }
    }
    if (cant_a_producir <= 0) { 
        comando_generador(argv[0]); return 1; 
    }

    if (ipc_abrir_todos(0, 0) < 0) 
        matar("ipc_abrir_todos(crear) fallo");

    while (cant_producida < cant_a_producir) {
        uint32_t base = 0, cont = 0;
        int no_disponible = pedir_bloque_ids(&base, &cont); // pido un bloque de IDs
        
        if (no_disponible) 
            break; // ya no quedan IDs globalmente

        for (uint32_t i=0; i < cont && cant_producida < cant_a_producir ;i++){ // para cada ID del bloque
            registro_t r;
            generar_randrec(&r, base + i, idx_generador); // genero un registro aleatorio
            push(&r); // lo pongo en el buffer circular (bloquea si está lleno)
            cant_producida++;
        }
    }

    ipc_cerrar_todos(0); // cierro referencias (no unlink)
    return 0;
}
