// include/ipc.h
// -----------------------------------------------------------------------------
// Propósito:
// - Encapsula la creación/apertura/limpieza de recursos IPC:
//   * SHM POSIX (shm_open/ftruncate/mmap)
//   * Semáforos POSIX nombrados (sem_open/sem_unlink)
// - Evita repetir lógica y centraliza el manejo de errores del SO.
// -----------------------------------------------------------------------------

#ifndef IPC_H
#define IPC_H

#include "common.h"

int    create_shm(const char *name, size_t total_size);
void  *map_shm(int fd, size_t total_size);
void   unlink_all(const names_t *n);

sems_t create_sems(const names_t *nn, size_t capacity);
sems_t open_sems(const names_t *nn);

#endif // IPC_H
