// include/common.h
// -----------------------------------------------------------------------------
// Definiciones y utilidades comunes para todo el proyecto.
// Se concentran aquí para:
//  - Evitar duplicación de includes y constantes.
//  - Exponer tipos compartidos (record_t, names_t, sems_t).
//  - Declarar funciones de ayuda (die, perr, rand_seed, fill_random_record, gen_names).
// -----------------------------------------------------------------------------

#ifndef COMMON_H
#define COMMON_H

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>
#include <semaphore.h>
#include <signal.h>

// -----------------------------------------------------------------------------
// Parámetros por defecto y tamaños de campos de ejemplo
// -----------------------------------------------------------------------------
#define DEFAULT_BUF_CAP 64   // capacidad por defecto del ring buffer
#define MAX_NAME        60   // tamaño de nombree evento
#define MAX_EST        12   // tamaño de campo "estado" 

// -----------------------------------------------------------------------------
// Estructura de registro: lo que viaja por SHM y se persiste en CSV
// Requisito: ID debe ir primero en el CSV (lo cumplimos al imprimir).
// -----------------------------------------------------------------------------
typedef struct {
    int  id;                      // Requisito: primera columna del CSV, asignado por el coordinador
    int  prioridad;               // posición actual en la cola
    char nombreUser[MAX_NAME];    // nombre de la persona que va a asistir al evento
    char evento[MAX_NAME];        // nombre de evento a asitir
    char estado[MAX_EST];         // "Esperando", "Atendido" o "Cancelado"
} record_t;

// -----------------------------------------------------------------------------
// Nombres de objetos POSIX (SHM + semáforos)
// - Se generan únicos por ejecución (PID + random) para evitar colisiones.
// -----------------------------------------------------------------------------
typedef struct {
    char shm_name[64];
    char sem_empty_name[64];
    char sem_full_name[64];
    char sem_mutex_name[64];
    char sem_id_name[64];
} names_t;

// -----------------------------------------------------------------------------
// Handle de semáforos POSIX
// -----------------------------------------------------------------------------
typedef struct {
    sem_t *empty;   // cuenta de lugares libres en la cola
    sem_t *full;    // cuenta de elementos disponibles
    sem_t *mutex;   // exclusión mutua para sección crítica del buffer
    sem_t *idlock;  // exclusión para asignación de bloques de IDs
} sems_t;

// -----------------------------------------------------------------------------
// Utilidades (implementadas en common.c)
// -----------------------------------------------------------------------------
void   die(const char *fmt, ...) __attribute__((format(printf,1,2)));
void   perr(const char *msg);
void   rand_seed(void);
void   fill_random_record(record_t *r, int id);
void   gen_names(names_t *n);

#endif // COMMON_H
