// include/ring.h
// -----------------------------------------------------------------------------
// Estructura de la región de SHM y primitivas de la cola circular:
//  - ring_push (productor)
//  - ring_pop  (consumidor)
// Además, reserva atómica de bloques de IDs de 10 en 10:
//  - request_id_block
// Prioridades (request_prio_block) 
// -----------------------------------------------------------------------------

#ifndef RING_H
#define RING_H

#include "common.h"

// Región compartida (cabecera + buffer flexible)
typedef struct {
    size_t   capacity;   // capacidad del buffer (N slots)
    size_t   head;       // índice de consumo
    size_t   tail;       // índice de producción

    // Asignaciones atómicas para identidad y posición de la cola
    int      next_id;    // próximo id a asignar (arranca en 1)
    int      next_prio;  // próxima prioridad a asignar (arranca en 1)
    int      total;      // total de registros a generar (tope)

    int      stop;       // bandera opcional para señalización/terminación

    record_t buf[];      // cola circular (arreglo flexible)
} shm_region_t;

// Primitivas bloqueantes (sin espera ocupada)
void ring_push(shm_region_t *shm, sems_t *s, const record_t *rec);
void ring_pop (shm_region_t *shm, sems_t *s,       record_t *out);

// Reserva atómica de bloques (máx 10, último bloque puede ser <10)
bool request_id_block   (shm_region_t *shm, sems_t *s, int *start, int *count);
bool request_prio_block (shm_region_t *shm, sems_t *s, int *start, int *count);

#endif // RING_H
