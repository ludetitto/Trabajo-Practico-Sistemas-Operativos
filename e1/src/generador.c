#include "ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <time.h>

extern void make_random_record(record_t* r, uint64_t id);

int main(int argc, char** argv){
    long long quota=-1;
    int opt;
    while((opt=getopt(argc,argv,"q:h"))!=-1){
        if(opt=='q') quota=atoll(optarg);
        else { printf("Uso: %s -q <registros>\n", argv[0]); return 1; }
    }
    if (quota<=0){ fprintf(stderr,"Parámetros inválidos.\n"); return 1; }

    srand((unsigned)time(NULL) ^ getpid());
    if (ipc_init(false)<0){ perror("ipc_connect"); return 2; }

    shm_queue_t* q = ipc_queue_map();
    shm_ids_t* ids = ipc_ids_map();

    long long done=0;
    while(done<quota){
        uint64_t base=0; int count=0;

        // "coordinador asigna": tomamos bloque de 10 bajo exclusión
        if (sem_wait(sem_mutex_g)<0) break;
        if (ids->remaining>0){
            count = (ids->remaining >= 10)? 10 : (int)ids->remaining;
            base = ids->next_id;
            ids->next_id += count;
            ids->remaining -= count;
        }
        sem_post(sem_mutex_g);

        if (count==0) break; // no quedan IDs

        for(int i=0;i<count && done<quota;i++){
            record_t r; make_random_record(&r, base+i);
            if (queue_push(q, &r, sem_empty_g, sem_full_g, sem_mutex_g)<0){
                perror("queue_push"); goto end;
            }
            done++;
        }
    }
end:
    ipc_close(false);
    return 0;
}
