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

#define COLA_CAP 8       // tamaño del buffer circular
#define NOMBRE_MAXLEN 64 // longitud de nombres aleatorios
#define TIMESTAMP_MAXLEN 20

// nombres POSIX de objetos compartidos
#define SHM_RING_NAME "/tp_ring"       // buffer circular
#define SHM_IDS_NAME "/tp_ids"         // estado de IDs
#define SEM_EMPTY_NAME "/tp_sem_empty" // semáforo de huecos vacíos
#define SEM_FULL_NAME "/tp_sem_full"   // semáforo de huecos ocupados
#define SEM_MUTEX_NAME "/tp_sem_mutex" // semáforo mutex para buffer circular
#define SEM_IDS_NAME "/tp_sem_ids"     // semáforo mutex para estado de IDs

#ifndef MAX_PRODS
#define MAX_PRODS 64
#endif

typedef struct
{
    uint32_t id;
    int generador;
    pid_t pid;
    char nombre[NOMBRE_MAXLEN];
    float precio;   // [1000.00, 100000.00]
    uint32_t stock; // [0, 100]
    char timestamp[TIMESTAMP_MAXLEN];
    bool borrado;
} registro_t;

typedef struct
{
    registro_t buffer[COLA_CAP];
    uint32_t primero;            // próximo pop
    uint32_t ultimo;             // próximo push
    uint32_t cant_elem_ocupados; // elementos ocupados
} cola_t;

/* ===== ids_t EXTENDIDO para RR estricto y tolerancia a fallos =====
   - única fuente de verdad para asignación de IDs
   - metadatos para round-robin y estado de hijos
*/
typedef struct
{
    uint32_t proximo;         // siguiente ID a entregar (arranca en 1)
    uint32_t restantes;       // IDs que faltan globalmente
    int nprods;               // cantidad total de generadores
    int turno;                // índice RR actual [0..nprods-1]
    pid_t pid[MAX_PRODS];     // PID de cada generador
    uint8_t alive[MAX_PRODS]; // 1 = vivo, 0 = muerto (lo marca el padre)
    uint8_t muerte_temprana;  // 1 si hubo alguna muerte antes de agotar IDs
} ids_t;

static inline void matar(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(EXIT_FAILURE);
}

#endif
