// src/ring.c
// -----------------------------------------------------------------------------
// Cola circular (productor/consumidor) y reserva atómica de bloques para
// IDs y PRIORIDADES. Semáforos bloqueantes: sin espera ocupada.
// -----------------------------------------------------------------------------

#include "ring.h"

void ring_push(shm_region_t *shm, sems_t *s, const record_t *rec) {
    sem_wait(s->empty);   // esperar lugar libre
    sem_wait(s->mutex);   // entrar a sección crítica

    shm->buf[shm->tail] = *rec;
    shm->tail = (shm->tail + 1) % shm->capacity;

    sem_post(s->mutex);   // salir de sección crítica
    sem_post(s->full);    // anunciar elemento disponible
}

void ring_pop(shm_region_t *shm, sems_t *s, record_t *out) {
    sem_wait(s->full);    // esperar elemento disponible
    sem_wait(s->mutex);   // entrar a sección crítica

    *out = shm->buf[shm->head];
    shm->head = (shm->head + 1) % shm->capacity;

    sem_post(s->mutex);   // salir de sección crítica
    sem_post(s->empty);   // anunciar lugar libre
}

// Reserva atómica de un bloque de IDs (máx 10; el último puede ser <10)
bool request_id_block(shm_region_t *shm, sems_t *s, int *start, int *count) {
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

// Reserva atómica de un bloque de PRIORIDADES (máx 10; el último puede ser <10)
// Reusa el mismo semáforo idlock para serializar esta asignación.
bool request_prio_block(shm_region_t *shm, sems_t *s, int *start, int *count) {
    bool ok = false;
    sem_wait(s->idlock);
    if (shm->next_prio <= shm->total) {
        *start = shm->next_prio;
        int rem = shm->total - shm->next_prio + 1;
        *count = (rem >= 10) ? 10 : rem;
        shm->next_prio += *count;
        ok = true;
    }
    sem_post(s->idlock);
    return ok;
}
