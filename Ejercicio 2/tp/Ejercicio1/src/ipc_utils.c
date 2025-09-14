#define _POSIX_C_SOURCE 200809L
#include "comun.h"

/* Crea (o reemplaza) la SHM y la mapea. Devuelve puntero mapeado. */
ShmQueue* create_and_map_shm(const char *name, size_t size) {
    /* limpiar si existen restos */
    shm_unlink(name); /* ignorar errores */

    int fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if (fd < 0) {
        perror("shm_open create");
        return NULL;
    }
    if (ftruncate(fd, size) < 0) {
        perror("ftruncate");
        close(fd);
        return NULL;
    }
    void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap create");
        close(fd);
        return NULL;
    }
    close(fd);
    return (ShmQueue*)addr;
}

/* Abrir y mapear una SHM ya creada (por el coordinador) */
ShmQueue* open_and_map_shm(const char *name, size_t size) {
    int fd = shm_open(name, O_RDWR, 0);
    if (fd < 0) {
        /* shm_open falló; retornamos NULL para que el llamador lo maneje */
        return NULL;
    }
    void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap open");
        close(fd);
        return NULL;
    }
    close(fd);
    return (ShmQueue*)addr;
}

/* Unlink de semáforos y shm; usado por coordinador al final */
void unlink_ipc(void) {
    sem_unlink(SEM_EMPTY_NAME);
    sem_unlink(SEM_FULL_NAME);
    sem_unlink(SEM_MUTEX_NAME);
    sem_unlink(SEM_ID_NAME);
    shm_unlink(SHM_NAME);
}






