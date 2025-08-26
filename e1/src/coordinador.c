#include "ipc.h"
#include "csv.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <getopt.h>

static volatile int stop=0;
static void on_sig(int s){ (void)s; stop=1; }

extern void make_random_record(record_t* r, uint64_t id);

int main(int argc, char** argv){
    int N=-1; long long TOTAL=-1; const char* out="db.csv";
    int opt;
    while((opt=getopt(argc,argv,"n:t:f:h"))!=-1){
        if(opt=='n') N=atoi(optarg);
        else if(opt=='t') TOTAL=atoll(optarg);
        else if(opt=='f') out=optarg;
        else { printf("Uso: %s -n <gen> -t <total> -f <csv>\n", argv[0]); return 1; }
    }
    if (N<=0 || TOTAL<=0){ fprintf(stderr,"Parámetros inválidos.\n"); return 1; }

    signal(SIGINT,on_sig); signal(SIGTERM,on_sig);
    if (ipc_init(true)<0){ perror("ipc_init"); return 2; }

    shm_ids_t* ids = ipc_ids_map();
    ids->next_id = 1;
    ids->remaining = (uint64_t)TOTAL;

    FILE* f = csv_open(out, 1);
    if(!f){ perror("csv_open"); ipc_close(true); return 3; }

    shm_queue_t* q = ipc_queue_map();

    long long consumidos = 0;
    record_t r;
    while(!stop && consumidos < TOTAL){
        if (queue_pop(q, &r, sem_empty_g, sem_full_g, sem_mutex_g)<0) break;
        if (csv_append(f,&r)<0){ perror("csv_append"); break; }
        consumidos++;
    }

    csv_close(f);
    ipc_close(true);
    return 0;
}
