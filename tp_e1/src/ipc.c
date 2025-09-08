// src/ipc.c
// -----------------------------------------------------------------------------
// Propósito del archivo:
// - Encapsular la creación, mapeo, apertura y unlink de SHM y semáforos POSIX.
// - Deja el main y el resto de módulos con menos "ruido" de syscalls.
// -----------------------------------------------------------------------------

#include "ipc.h"

int create_shm(const char *name, size_t total_size) {
    int fd = shm_open(name, O_CREAT|O_EXCL|O_RDWR, 0600);
    if (fd == -1) perr("shm_open");
    if (ftruncate(fd, (off_t)total_size) == -1) perr("ftruncate");
    return fd;
}

void *map_shm(int fd, size_t total_size) {
    void *p = mmap(NULL, total_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) perr("mmap");
    return p;
}

void unlink_all(const names_t *n) {
    shm_unlink(n->shm_name);
    sem_unlink(n->sem_empty_name);
    sem_unlink(n->sem_full_name);
    sem_unlink(n->sem_mutex_name);
    sem_unlink(n->sem_id_name);
}

sems_t create_sems(const names_t *nn, size_t capacity) {
    sems_t s;
    s.empty = sem_open(nn->sem_empty_name, O_CREAT|O_EXCL, 0600, capacity);
    if (s.empty == SEM_FAILED) perr("sem_open empty");
    s.full  = sem_open(nn->sem_full_name,  O_CREAT|O_EXCL, 0600, 0);
    if (s.full  == SEM_FAILED) perr("sem_open full");
    s.mutex = sem_open(nn->sem_mutex_name, O_CREAT|O_EXCL, 0600, 1);
    if (s.mutex == SEM_FAILED) perr("sem_open mutex");
    s.idlock= sem_open(nn->sem_id_name,    O_CREAT|O_EXCL, 0600, 1);
    if (s.idlock== SEM_FAILED) perr("sem_open idlock");
    return s;
}

sems_t open_sems(const names_t *nn) {
    sems_t s;
    s.empty = sem_open(nn->sem_empty_name, 0);
    s.full  = sem_open(nn->sem_full_name,  0);
    s.mutex = sem_open(nn->sem_mutex_name, 0);
    s.idlock= sem_open(nn->sem_id_name,    0);
    if (s.empty==SEM_FAILED || s.full==SEM_FAILED ||
        s.mutex==SEM_FAILED || s.idlock==SEM_FAILED) {
        perr("sem_open (open)");
    }
    return s;
}
