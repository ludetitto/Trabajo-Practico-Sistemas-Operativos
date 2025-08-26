#pragma once
#include "common.h"
#include <semaphore.h>
#include <stdbool.h>

typedef struct {
    size_t head, tail;
    record_t ring[RING_B];
} shm_queue_t;

#define SHM_QUEUE_NAME "/tp_queue_e1"
#define SEM_EMPTY_NAME "/tp_empty_e1"
#define SEM_FULL_NAME  "/tp_full_e1"
#define SEM_MUTEX_NAME "/tp_mutex_e1"
#define SHM_IDS_NAME   "/tp_ids_e1"

typedef struct {
    uint64_t next_id;    // lo incrementa el coordinador
    uint64_t remaining;  // registros por asignar
} shm_ids_t;

int  ipc_init(bool create);
void ipc_close(bool destroy);
shm_queue_t* ipc_queue_map(void);
shm_ids_t*   ipc_ids_map(void);
int  queue_push(shm_queue_t* q, const record_t* r, sem_t* empty, sem_t* full, sem_t* mutex);
int  queue_pop (shm_queue_t* q, record_t* r, sem_t* empty, sem_t* full, sem_t* mutex);

extern sem_t *sem_empty_g, *sem_full_g, *sem_mutex_g;
