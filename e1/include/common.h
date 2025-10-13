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

/* ===== ids_t EXTENDIDO: RR + tolerancia a fallos + “devoluciones” =====
   - única fuente de verdad para asignación de IDs
   - metadatos de RR y estado de hijos
   - seguimiento de bloque vigente por generador
   - cola de rangos “devueltos” por muertes (para no dejar huecos)
*/

// rango devuelto (IDs pendientes de algún bloque a medio terminar)
typedef struct
{
    uint32_t base;
    uint32_t cant;
} rango_t;

#ifndef DEVQ_CAP
#define DEVQ_CAP 256 // capacidad de cola de devoluciones
#endif

typedef struct
{
    // Pool de IDs “nuevos”
    uint32_t proximo;   // siguiente ID nuevo a asignar (arranca en 1)
    uint32_t restantes; // cuántos IDs nuevos faltan globalmente
    int nprods;         // cantidad total de generadores publicados
    int turno;          // índice RR actual [0..nprods-1]

    // Tabla de hijos
    pid_t pid[MAX_PRODS];
    int alive[MAX_PRODS]; // 1 = vivo, 0 = muerto

    // Bloque vigente por generador
    uint32_t bloq_base[MAX_PRODS];   // inicio del bloque
    uint32_t bloq_cant[MAX_PRODS];   // tamaño del bloque (<=10)
    uint32_t bloq_avance[MAX_PRODS]; // producidos dentro del bloque
    int bloq_activo[MAX_PRODS];      // 1 si tiene bloque vigente

    // Cola de “devoluciones” (rangos a reasignar primero)
    rango_t devq[DEVQ_CAP];
    int devq_head; // índice de pop
    int devq_tail; // índice de push
    int devq_len;  // cantidad de elementos en cola
} ids_t;

// Señalador de que ids_t ya está definido acá (para que ipc.h no lo redefina)
#define IDS_T_DEFINED_IN_COMMON_H 1

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
