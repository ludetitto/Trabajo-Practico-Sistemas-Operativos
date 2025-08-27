#include "../include/common.h"
#include "../include/ipc.h"
#include "../include/randrec.h"

static void usage(const char* prog){ // Help command
    fprintf(stderr,"Uso: %s -q <cantidad_por_generador> -g <idx>\n", prog);
}

int main(int argc, char** argv){
    int q = -1; int gidx = 0;
    for (int i=1;i<argc;i++){ // Parseo de argumentos
        if (!strcmp(argv[i],"-q") && i+1<argc) q = atoi(argv[++i]);
        else if (!strcmp(argv[i],"-g") && i+1<argc) gidx = atoi(argv[++i]);
        else if (!strcmp(argv[i],"--help")) { usage(argv[0]); return 0; }
    }
    if (q<=0){ usage(argv[0]); return 1; }

    if (ipc_open_all(0, 0) < 0) die("ipc_open_all(open) fallo");

    int produced = 0;
    while (produced < q) {
        uint32_t base=0, cnt=0;
        int no_more = ids_take_block(&base, &cnt); // pido un bloque de IDs
        if (no_more) break; // ya no quedan IDs globalmente

        for (uint32_t i=0;i<cnt && produced<q;i++){ // para cada ID del bloque
            record_t r;
            randrec_make(&r, base+i, gidx); // genero un registro aleatorio
            ring_push(&r); // lo pongo en el buffer circular (bloquea si está lleno)
            produced++;
        }
    }

    ipc_close_all(0); // cierro referencias (no unlink)
    return 0;
}
