// src/ring.c
// -----------------------------------------------------------------------------
// Implementación del ring buffer de productor/consumidor y reserva de IDs
// -----------------------------------------------------------------------------

#include "ring.h"

// Productor: deposita un registro en la cola circular
void ring_push(shm_region_t *shm, sems_t *s, const record_t *rec) {
    // P(empty): esperar un lugar libre
    sem_wait(s->empty);
    // P(mutex): entrar a sección crítica
    sem_wait(s->mutex);

    // Escribir en posición tail y avanzar tail (mod capacity)
    shm->buf[shm->tail] = *rec;
    shm->tail = (shm->tail + 1) % shm->capacity;

    // V(mutex): salir de sección crítica
    sem_post(s->mutex);
    // V(full): anunciar un elemento disponible
    sem_post(s->full);
}

// Consumidor: extrae un registro de la cola circular
void ring_pop(shm_region_t *shm, sems_t *s, record_t *out) {
    // P(full): esperar que haya al menos un elemento
    sem_wait(s->full);
    // P(mutex): entrar a sección crítica
    sem_wait(s->mutex);

    // Leer desde head y avanzar head
    *out = shm->buf[shm->head];
    shm->head = (shm->head + 1) % shm->capacity;

    // V(mutex): salir de sección crítica
    sem_post(s->mutex);
    // V(empty): anunciar un lugar libre
    sem_post(s->empty);
}

// Reserva atómicamente un bloque de IDs (máx 10). Devuelve false si no quedan.
bool request_id_block(shm_region_t *shm, sems_t *s, int *start, int *count) {
    bool ok = false;

    // Serializamos la asignación de IDs con un semáforo dedicado (idlock).
    sem_wait(s->idlock);

    if (shm->next_id <= shm->total) {
        *start = shm->next_id;
        int rem = shm->total - shm->next_id + 1;
        *count = (rem >= 10) ? 10 : rem;  // último bloque puede ser menor a 10
        shm->next_id += *count;
        ok = true;
    }

    sem_post(s->idlock);
    return ok;
}
