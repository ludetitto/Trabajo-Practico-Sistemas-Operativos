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
#include <locale.h>

#define COLA_CAP 8       // tamaño del buffer circular
#define NOMBRE_MAXLEN 64
#define TIMESTAMP_MAXLEN 20

// nombres POSIX de objetos compartidos
#define SHM_RING_NAME "/tp_ring"
#define SHM_IDS_NAME  "/tp_ids"
#define SEM_EMPTY_NAME "/tp_sem_empty"
#define SEM_FULL_NAME  "/tp_sem_full"
#define SEM_MUTEX_NAME "/tp_sem_mutex"
#define SEM_IDS_NAME   "/tp_sem_ids"

#ifndef MAX_PRODS
#define MAX_PRODS 64
#endif

typedef struct {
    uint32_t id;
    int      generador;
    pid_t    pid;
    char     nombre[NOMBRE_MAXLEN];
    float    precio;
    uint32_t stock;
    char     timestamp[TIMESTAMP_MAXLEN];
    bool     borrado;
} registro_t;

typedef struct {
    registro_t buffer[COLA_CAP];
    uint32_t   primero;
    uint32_t   ultimo;
    uint32_t   cant_elem_ocupados;
} cola_t;

/* ===== ids_t EXTENDIDO: RR + tolerancia a fallos (sin huecos) =====
   - RR estricto entre generadores vivos
   - seguimiento del bloque activo por generador
   - cola de devoluciones (IDs de bloques inconclusos) para reasignar primero
*/
typedef struct { uint32_t base, cant; } rango_t;

#ifndef DEVQ_CAP
#define DEVQ_CAP 256
#endif

typedef struct {
    // Pool de IDs “nuevos”
    uint32_t proximo;         // siguiente ID a entregar (arranca en 1)
    uint32_t restantes;       // IDs nuevos que faltan
    int      nprods;          // cant. total de generadores
    int      turno;           // índice RR actual [0..nprods-1]

    // Tabla de hijos
    pid_t    pid[MAX_PRODS];
    uint8_t  alive[MAX_PRODS];      // 1=vivo, 0=muerto

    // Bloque vigente por generador
    uint32_t bloq_base[MAX_PRODS];   // inicio del bloque
    uint32_t bloq_cant[MAX_PRODS];   // tamaño (<=10)
    uint32_t bloq_avance[MAX_PRODS]; // cuántos ya produjo del bloque
    uint8_t  bloq_activo[MAX_PRODS]; // 1=hay bloque vigente

    // Cola de devoluciones (rangos a reasignar primero)
    rango_t  devq[DEVQ_CAP];
    int      devq_head, devq_tail, devq_len;
} ids_t;

static inline void matar(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(EXIT_FAILURE);
}

#endif
