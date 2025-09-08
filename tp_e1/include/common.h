// include/common.h
// -----------------------------------------------------------------------------
// Propósito:
// - Tipos, constantes y utilidades comunes a todo el proyecto.
// - Plan de negocio e-commerce: CSV -> id,generador,pid,nombreProducto,precio,stock
// - Provee record_t (estructura de un registro), names_t (nombres POSIX),
//   sems_t (handle de semáforos) y helpers (errores, RNG, generación de registros).
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
#include <locale.h>   // para asegurar punto decimal en CSV

// -----------------------------------------------------------------------------
// Parámetros por defecto y tamaños apropiados
// -----------------------------------------------------------------------------
#define DEFAULT_BUF_CAP 8        // capacidad por defecto del ring buffer
#define MAX_NAME        40        // tamaño para nombre del producto

// -----------------------------------------------------------------------------
// Estructura de registro que viaja por SHM y se persiste en CSV.
// Requisito: id es la PRIMERA columna del CSV.
// -----------------------------------------------------------------------------
typedef struct {
    int    id;                    // 1ra col. del CSV (identificador único global)
    int    generador;             // índice lógico del generador (0..N-1)
    pid_t  pid;                   // PID del proceso generador
    char   nombreProducto[MAX_NAME];
    double precio;                // precio unitario (ej.: 159999.99)
    int    stock;                 // unidades disponibles (>=0)
} record_t;

// -----------------------------------------------------------------------------
// Nombres de objetos POSIX (SHM + semáforos)
// -----------------------------------------------------------------------------
typedef struct {
    char shm_name[64];
    char sem_empty_name[64];
    char sem_full_name[64];
    char sem_mutex_name[64];
    char sem_id_name[64];   // serializa asignación de bloques de IDs
} names_t;

// -----------------------------------------------------------------------------
// Handle de semáforos POSIX
// -----------------------------------------------------------------------------
typedef struct {
    sem_t *empty;   // lugares libres en la cola
    sem_t *full;    // elementos disponibles
    sem_t *mutex;   // exclusión mutua del buffer
    sem_t *idlock;  // exclusión para asignación de IDs
} sems_t;

// -----------------------------------------------------------------------------
// Utilidades (implementadas en common.c)
// -----------------------------------------------------------------------------
void   die(const char *fmt, ...) __attribute__((format(printf,1,2)));
void   perr(const char *msg);
void   rand_seed(void);

// Genera un registro de ejemplo coherente con el negocio e-commerce.
// Nota: 'generador' y 'pid' los setea el productor antes de enviar.
void   fill_random_product_fields(record_t *r);

// Genera nombres únicos para recursos POSIX (evita colisiones entre corridas).
void   gen_names(names_t *n);
//comando de ayuda
void   print_help_examples(const char *prog);


#endif // COMMON_H
