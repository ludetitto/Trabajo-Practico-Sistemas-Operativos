// src/ring.c
// -----------------------------------------------------------------------------
// Propósito del archivo:
// - Implementa la cola circular (ring buffer) para el patrón productor/consumidor.
// - Provee ring_push/ring_pop con semáforos bloqueantes (sin busy-wait).
// - Implementa la reserva atómica de bloques de IDs (10 en 10).
// -----------------------------------------------------------------------------

#include "ring.h"

void ring_push(shm_region_t *shm, sems_t *s, const record_t *rec) {
    // Productor: P(empty) -> P(mutex) -> push -> V(mutex) -> V(full)
    sem_wait(s->empty);
    sem_wait(s->mutex);

    shm->buf[shm->tail] = *rec;
    shm->tail = (shm->tail + 1) % shm->capacity;

    sem_post(s->mutex);
    sem_post(s->full);
}

void ring_pop(shm_region_t *shm, sems_t *s, record_t *out) {
    // Consumidor: P(full) -> P(mutex) -> pop -> V(mutex) -> V(empty)
    sem_wait(s->full);
    sem_wait(s->mutex);

    *out = shm->buf[shm->head];
    shm->head = (shm->head + 1) % shm->capacity;

    sem_post(s->mutex);
    sem_post(s->empty);
}

bool request_id_block(shm_region_t *shm, sems_t *s, int *start, int *count) {
    // Reserva atómica de IDs correlativos (bloques de 10; el último puede ser menor)
    bool ok = false;
    sem_wait(s->idlock);

    if (shm->next_id <= shm->total) {
        *start = shm->next_id;
        int rem = shm->total - shm->next_id + 1;
        *count = (rem >= 10) ? 10 : rem;
        shm->next_id += *count;
        ok = true;
    }

    sem_post(s->idlock);
    return ok;
}
