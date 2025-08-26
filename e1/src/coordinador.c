#include "../include/common.h"
#include "../include/ipc.h"
#include "../include/csv.h"

static volatile sig_atomic_t stop = 0;
static void on_sig(int s){ (void)s; stop = 1; }

static void usage(const char* prog){
    fprintf(stderr,
      "Uso: %s -n <generadores> -t <total_registros> -f <csv>\n", prog);
}

int main(int argc, char** argv){
    int N = -1; int total = -1; const char* csvpath = NULL;

    for (int i=1;i<argc;i++){
        if (!strcmp(argv[i],"-n") && i+1<argc) N = atoi(argv[++i]);
        else if (!strcmp(argv[i],"-t") && i+1<argc) total = atoi(argv[++i]);
        else if (!strcmp(argv[i],"-f") && i+1<argc) csvpath = argv[++i];
        else if (!strcmp(argv[i],"--help")) { usage(argv[0]); return 0; }
    }
    if (N<=0 || total<=0 || !csvpath){ usage(argv[0]); return 1; }

    signal(SIGINT,on_sig); signal(SIGTERM,on_sig);

    if (ipc_open_all(1, (uint32_t)total) < 0) die("ipc_open_all(create) fallo");

    FILE* f = csv_open(csvpath, 1);
    if (!f) { ipc_close_all(1); die("No pude abrir CSV: %s", csvpath); }

    uint32_t written = 0;
    while (!stop && written < (uint32_t)total) {
        record_t r;
        if (ring_pop(&r)==0) {
            csv_write(f, &r);
            written++;
        } else {
            // nada para consumir, dormir un poquito
            struct timespec ts = {.tv_sec=0, .tv_nsec=1000000};
            nanosleep(&ts, NULL);
        }
    }

    csv_close(f);
    ipc_close_all(1); // liberar recursos
    return 0;
}
