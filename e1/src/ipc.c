#include "../include/ipc.h"

// Memoria compartida
// globales para no tener que pasarlos como parámetro
// que son punteros a la memoria mapeada
ring_t*      g_ring = NULL; // buffer circular, sirve para pasar registros entre procesos
ids_state_t* g_ids  = NULL; // estado de los IDs, sirve para asignar IDs únicos

// Semáforos
// globales para no tener que pasarlos como parámetro
sem_t* sem_empty = NULL; // cuenta espacios vacíos en el buffer circular
sem_t* sem_full  = NULL; // cuenta elementos ocupados en el buffer circular
sem_t* sem_mutex = NULL; // mutex para acceso exclusivo al buffer circular
sem_t* sem_ids   = NULL; // mutex para acceso exclusivo al estado de los IDs

static int map_create_fd(const char* name, size_t sz, int create) { // create=1 para crear, 0 para abrir
    int oflag = create ? (O_CREAT|O_RDWR) : O_RDWR;
    int fd = shm_open(name, oflag, 0666); // permisos rw-rw-rw de la biblioteca 
    // shm es como un archivo, pero en memoria compartida
    // sirve para compartir memoria entre procesos
    if (fd < 0) return -1;
    if (create) {
        if (ftruncate(fd, (off_t)sz) < 0) { close(fd); return -1; } // darle tamaño
    }
    return fd;
}

int ipc_open_all(int create, uint32_t total_ids) { // create=1 para crear, 0 para abrir
    // SHM ring
    int fd = map_create_fd(SHM_RING_NAME, sizeof(ring_t), create);
    if (fd < 0) return -1;
    g_ring = mmap(NULL, sizeof(ring_t), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0); // mapear en memoria
    close(fd);
    if (g_ring == MAP_FAILED) return -1;
    if (create) memset(g_ring, 0, sizeof(*g_ring)); // inicializar en cero

    // SHM ids
    fd = map_create_fd(SHM_IDS_NAME, sizeof(ids_state_t), create);
    if (fd < 0) return -1;
    g_ids = mmap(NULL, sizeof(ids_state_t), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (g_ids == MAP_FAILED) return -1;
    if (create) {
        g_ids->next_id  = 1;
        g_ids->remaining = total_ids; // cantidad total de IDs que se van a pedir
    }

    // Semáforos
    if (create) {
        sem_unlink(SEM_EMPTY_NAME); // eliminar si ya existían
        sem_unlink(SEM_FULL_NAME);
        sem_unlink(SEM_MUTEX_NAME);
        sem_unlink(SEM_IDS_NAME);
    }
    sem_empty = sem_open(SEM_EMPTY_NAME, create? O_CREAT:0, 0666, RING_CAP); // espacios vacíos = capacidad del buffer
    sem_full  = sem_open(SEM_FULL_NAME , create? O_CREAT:0, 0666, 0); // elementos ocupados = 0
    sem_mutex = sem_open(SEM_MUTEX_NAME, create? O_CREAT:0, 0666, 1); // mutex binario
    sem_ids   = sem_open(SEM_IDS_NAME  , create? O_CREAT:0, 0666, 1); // mutex binario
    if (!sem_empty || !sem_full || !sem_mutex || !sem_ids) return -1;

    return 0;
}

void ipc_close_all(int unlink_now) {
    if (g_ring) munmap(g_ring, sizeof(ring_t)); // liberar memoria mapeada
    if (g_ids)  munmap(g_ids,  sizeof(ids_state_t));

    if (sem_empty) sem_close(sem_empty); // liberar semáforos
    if (sem_full)  sem_close(sem_full);
    if (sem_mutex) sem_close(sem_mutex);
    if (sem_ids)   sem_close(sem_ids);

    if (unlink_now) {
        shm_unlink(SHM_RING_NAME); // eliminar memoria compartida
        shm_unlink(SHM_IDS_NAME);
        sem_unlink(SEM_EMPTY_NAME);
        sem_unlink(SEM_FULL_NAME);
        sem_unlink(SEM_MUTEX_NAME);
        sem_unlink(SEM_IDS_NAME);
    }
}

void ring_push(const record_t* r) { // bloquea hasta que haya espacio
    // pone un registro en el buffer circular
    sem_wait(sem_empty); // P
    sem_wait(sem_mutex); // P
    g_ring->buf[g_ring->tail] = *r; // copia el registro
    g_ring->tail = (g_ring->tail + 1) % RING_CAP; // avanza el índice circular
    g_ring->count++; // un elemento más
    sem_post(sem_mutex); // V
    sem_post(sem_full); // V
}

/// COORDINACIÓN CLÁSICA PRODUCTOR-CONSUMIDOR
// Push / Pop con semáforos: Uno producto y muchos consumen.

int ring_pop(record_t* out) {
    // devuelve 0 si obtuvo un registro, -1 si no hay (debería coordinarse con 'remaining')
    if (sem_trywait(sem_full) != 0) return -1; // no hay elementos ocupados
    // hay al menos un elemento, puedo sacar
    sem_wait(sem_mutex); // P
    *out = g_ring->buf[g_ring->head];
    g_ring->head = (g_ring->head + 1) % RING_CAP; // avanza el índice circular
    g_ring->count--; // un elemento menos
    sem_post(sem_mutex); // V
    sem_post(sem_empty); // V
    return 0;
}

int ids_take_block(uint32_t* base, uint32_t* count) {
    // asigna hasta 10 IDs; devuelve 0 si asignó, 1 si ya no quedan.
    sem_wait(sem_ids); // P
    // cuántos doy? hasta 10, o los que queden si son menos
    uint32_t give = (g_ids->remaining > 10) ? 10 : g_ids->remaining;
    if (give == 0) {
        sem_post(sem_ids); // V
        *base = 0; *count = 0;
        return 1; // no quedan
    }
    *base  = g_ids->next_id; // primer ID a entregar
    *count = give; // cuántos IDs entrego
    g_ids->next_id  += give;
    g_ids->remaining -= give;
    sem_post(sem_ids); // V
    return 0;
}
