#ifndef COMUN_H
#define COMUN_H

/* Habilitar funciones POSIX modernas */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/wait.h>

/* Ajusta la capacidad de la cola según necesidades */
#define QUEUE_CAP 100
#define BLOCK_SIZE 10

/* Nombres POSIX (SHM y semáforos con prefijo) */
#define SHM_NAME "/tp_shm_queue_2025"
#define SEM_EMPTY_NAME "/tp_sem_empty_2025"
#define SEM_FULL_NAME  "/tp_sem_full_2025"
#define SEM_MUTEX_NAME "/tp_sem_mutex_2025"
#define SEM_ID_NAME    "/tp_sem_id_2025"

/* Campos */
#define MAX_NAME 64
#define MAX_TS   32

typedef struct {
    int id;
    char name[MAX_NAME];
    double price;
    int stock;
    pid_t generator_pid;
    char ts[MAX_TS];
} Product;

/* Estructura en SHM: cola circular + contador global + meta */
typedef struct {
    Product buf[QUEUE_CAP];
    int head;
    int tail;
    int count;
    int next_id;    /* siguiente id disponible (global) */
    int total_T;    /* total objetivo */
} ShmQueue;

/* Utilidad */
static inline void perror_exit(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

/* Prototipos de utilidades IPC (implementados en ipc_utils.c) */
ShmQueue* create_and_map_shm(const char *name, size_t size);
ShmQueue* open_and_map_shm(const char *name, size_t size);
void unlink_ipc(void);

#endif /* COMUN_H */








