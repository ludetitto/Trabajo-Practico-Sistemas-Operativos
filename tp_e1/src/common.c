// src/common.c
// -----------------------------------------------------------------------------
// Utilidades comunes: errores, RNG, generación de registros y nombres POSIX.
// -----------------------------------------------------------------------------

#include "common.h"

void die(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    exit(EXIT_FAILURE);
}

void perr(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE); //hola cambio
}

void rand_seed(void) {
    unsigned s = (unsigned)time(NULL) ^ (unsigned)getpid() ^ (unsigned)getppid();
    srand(s);
}

// Listas de ejemplo para campos textuales
static const char *NAMES[] = {
    "Ana","Luis","Luz","Juan","Sofi","Nico","Pia","Tomi","Lara","Rami",
    "Mia","Mate","Lolo","Iara","Gero","Vio","Ivo","Lau","Pau","Roc"
};
static const char *EVENTS[] = {
    "Charla","Meetup","Taller","Feria","Seminario",
    "Concierto","Expo","Workshop","Hackday","Keynote"
};

// Genera un registro coherente con la regla de negocio (snapshot de cola)
// - mark_first_attended=true marca prioridad==1 como "Atendido" (opcional).
void fill_random_record(record_t *r, int id, int prioridad, bool mark_first_attended) {
    r->id        = id;
    r->prioridad = prioridad;

    snprintf(r->nombreUser, MAX_NAME, "%s",
             NAMES[rand()% (int)(sizeof(NAMES)/sizeof(NAMES[0]))]);
    snprintf(r->evento, MAX_NAME, "%s",
             EVENTS[rand()% (int)(sizeof(EVENTS)/sizeof(EVENTS[0]))]);

    if (mark_first_attended && prioridad == 1) {
        snprintf(r->estado, MAX_EST, "Atendido");
    } else {
        snprintf(r->estado, MAX_EST, "Esperando");
    }
}

// Nombres únicos para recursos POSIX (evitan colisiones entre corridas)
void gen_names(names_t *n) {
    int r = rand();
    snprintf(n->shm_name,       sizeof(n->shm_name),       "/pc_shm_%d_%d",   (int)getpid(), r);
    snprintf(n->sem_empty_name, sizeof(n->sem_empty_name), "/pc_empty_%d_%d", (int)getpid(), r);
    snprintf(n->sem_full_name,  sizeof(n->sem_full_name),  "/pc_full_%d_%d",  (int)getpid(), r);
    snprintf(n->sem_mutex_name, sizeof(n->sem_mutex_name), "/pc_mutex_%d_%d", (int)getpid(), r);
    snprintf(n->sem_id_name,    sizeof(n->sem_id_name),    "/pc_id_%d_%d",    (int)getpid(), r);
}
