// src/common.c
// -----------------------------------------------------------------------------
// Implementación de utilidades comunes declaradas en common.h
// -----------------------------------------------------------------------------

#include "common.h"

// Mensaje de error con formato + exit
void die(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    exit(EXIT_FAILURE);
}

// perror + exit (para errores de syscalls)
void perr(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

// Semilla de RNG distinta por proceso/hora (útil en generadores tras fork)
void rand_seed(void) {
    unsigned s = (unsigned)time(NULL) ^ (unsigned)getpid() ^ (unsigned)getppid();
    srand(s);
}

// Listas de ejemplo para campos textuales
static const char *NAMES[] = {
    "Ana","Luis","Luz","Juan","Sofi","Nico","Pia","Tomi","Lara","Rami",
    "Mia","Mate","Lolo","Iara","Gero","Vio","Ivo","Lau","Pau","Roc"
};
static const char *CITIES[] = {
    "BsAs","Cba","Ros","Mza","Tuc","Lpaz","Neu","Riv","Sal","Juj"
};

// Genera un registro con datos aleatorios coherentes para el CSV
void fill_random_record(record_t *r, int id) {
    r->id    = id;
    r->age   = 18 + rand()%73;     // 18..90
    r->score = rand()%101;         // 0..100
    snprintf(r->name, MAX_NAME, "%s",
             NAMES[rand()% (int)(sizeof(NAMES)/sizeof(NAMES[0]))]);
    snprintf(r->city, MAX_CITY, "%s",
             CITIES[rand()% (int)(sizeof(CITIES)/sizeof(CITIES[0]))]);
}

// Genera nombres únicos de recursos POSIX (para no colisionar entre corridas)
void gen_names(names_t *n) {
    int r = rand();
    snprintf(n->shm_name,       sizeof(n->shm_name),       "/pc_shm_%d_%d",   (int)getpid(), r);
    snprintf(n->sem_empty_name, sizeof(n->sem_empty_name), "/pc_empty_%d_%d", (int)getpid(), r);
    snprintf(n->sem_full_name,  sizeof(n->sem_full_name),  "/pc_full_%d_%d",  (int)getpid(), r);
    snprintf(n->sem_mutex_name, sizeof(n->sem_mutex_name), "/pc_mutex_%d_%d", (int)getpid(), r);
    snprintf(n->sem_id_name,    sizeof(n->sem_id_name),    "/pc_id_%d_%d",    (int)getpid(), r);
}
