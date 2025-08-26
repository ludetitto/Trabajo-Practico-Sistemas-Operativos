#define _POSIX_C_SOURCE 200809L


#include "ipc.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

sem_t *sem_empty_g=NULL, *sem_full_g=NULL, *sem_mutex_g=NULL;
static shm_queue_t* q_ptr = NULL;
static shm_ids_t*   ids_ptr = NULL;

static int map_object(const char* name, size_t sz, int oflag, void** out){
    int fd = shm_open(name, oflag, 0666);
    if (fd<0) return -1;
    if (oflag & O_CREAT) if (ftruncate(fd, sz)<0) { close(fd); return -1; }
    void* p = mmap(NULL, sz, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (p==MAP_FAILED) return -1;
    *out = p; return 0;
}

int ipc_init(bool create){
    int oflag = create ? (O_CREAT|O_RDWR) : O_RDWR;

    if (map_object(SHM_QUEUE_NAME, sizeof(shm_queue_t), oflag, (void**)&q_ptr)<0) return -1;
    if (create) memset(q_ptr, 0, sizeof(*q_ptr));

    if (map_object(SHM_IDS_NAME, sizeof(shm_ids_t), oflag, (void**)&ids_ptr)<0) return -1;
    if (create) memset(ids_ptr, 0, sizeof(*ids_ptr));

    unsigned e=RING_B, f=0, m=1;
    sem_empty_g = sem_open(SEM_EMPTY_NAME, create? O_CREAT:0, 0666, e);
    sem_full_g  = sem_open(SEM_FULL_NAME,  create? O_CREAT:0, 0666, f);
    sem_mutex_g = sem_open(SEM_MUTEX_NAME, create? O_CREAT:0, 0666, m);
    if (sem_empty_g==SEM_FAILED || sem_full_g==SEM_FAILED || sem_mutex_g==SEM_FAILED) return -1;
    return 0;
}

void ipc_close(bool destroy){
    if (q_ptr)   munmap(q_ptr,  sizeof(shm_queue_t));
    if (ids_ptr) munmap(ids_ptr, sizeof(shm_ids_t));
    if (sem_empty_g) sem_close(sem_empty_g);
    if (sem_full_g)  sem_close(sem_full_g);
    if (sem_mutex_g) sem_close(sem_mutex_g);
    if (destroy){
        shm_unlink(SHM_QUEUE_NAME);
        shm_unlink(SHM_IDS_NAME);
        sem_unlink(SEM_EMPTY_NAME);
        sem_unlink(SEM_FULL_NAME);
        sem_unlink(SEM_MUTEX_NAME);
    }
}

shm_queue_t* ipc_queue_map(void){ return q_ptr; }
shm_ids_t*   ipc_ids_map(void){   return ids_ptr; }

int queue_push(shm_queue_t* q, const record_t* r, sem_t* empty, sem_t* full, sem_t* mutex){
    if (sem_wait(empty)<0) return -1;
    if (sem_wait(mutex)<0){ sem_post(empty); return -1; }
    q->ring[q->head] = *r;
    q->head = (q->head + 1) % RING_B;
    sem_post(mutex);
    sem_post(full);
    return 0;
}
int queue_pop(shm_queue_t* q, record_t* r, sem_t* empty, sem_t* full, sem_t* mutex){
    if (sem_wait(full)<0) return -1;
    if (sem_wait(mutex)<0){ sem_post(full); return -1; }
    *r = q->ring[q->tail];
    q->tail = (q->tail + 1) % RING_B;
    sem_post(mutex);
    sem_post(empty);
    return 0;
}
