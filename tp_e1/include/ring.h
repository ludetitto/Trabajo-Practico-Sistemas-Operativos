// include/ring.h
// -----------------------------------------------------------------------------
// Propósito:
// - Define la región de SHM (cabecera + cola circular) y las primitivas P/C:
//   ring_push (productor) y ring_pop (consumidor).
// - Implementa la reserva atómica de bloques de IDs (máx 10) para garantizar
//   IDs correlativos, sin huecos ni duplicados, cumpliendo criterios de corrección.
// -----------------------------------------------------------------------------

#ifndef RING_H
#define RING_H

#include "common.h"

// Región compartida (layout fijo + arreglo flexible para la cola)
typedef struct {
    size_t   capacity;   // capacidad del buffer
    size_t   head;       // índice de consumo
    size_t   tail;       // índice de producción

    int      next_id;    // próximo ID a otorgar (arranca en 1)
    int      total;      // total de registros a generar
    int      stop;       // bandera opcional para terminación/SEÑALES

    record_t buf[];      // cola circular
} shm_region_t;

// Primitivas bloqueantes (sin espera ocupada)
void ring_push(shm_region_t *shm, sems_t *s, const record_t *rec);
void ring_pop (shm_region_t *shm, sems_t *s,       record_t *out);

// Reserva atómica de bloques de IDs (10 en 10; el último bloque puede ser <10)
bool request_id_block(shm_region_t *shm, sems_t *s, int *start, int *count);

#endif // RING_H
