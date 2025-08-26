#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>

#define RING_CAP 64    // tamaño del buffer circular
#define NAME_MAXLEN 32 // longitud de nombres aleatorios

// nombres POSIX (cambialos si querés correr varias instancias en paralelo)
#define SHM_RING_NAME "/tp_ring"
#define SHM_IDS_NAME "/tp_ids"
#define SEM_EMPTY_NAME "/tp_sem_empty"
#define SEM_FULL_NAME "/tp_sem_full"
#define SEM_MUTEX_NAME "/tp_sem_mutex"
#define SEM_IDS_NAME "/tp_sem_ids"

typedef struct
{
    uint32_t id;
    int generador;
    pid_t pid;
    char nombre[NAME_MAXLEN];
} record_t;

typedef struct
{
    record_t buf[RING_CAP];
    uint32_t head;  // próximo pop
    uint32_t tail;  // próximo push
    uint32_t count; // elementos ocupados
} ring_t;

typedef struct
{
    uint32_t next_id;   // siguiente ID a entregar
    uint32_t remaining; // IDs que faltan globalmente
} ids_state_t;

static inline void die(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(EXIT_FAILURE);
}

#endif
