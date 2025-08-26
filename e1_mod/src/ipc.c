#include "ipc.h"

ring_t*      g_ring = NULL;
ids_state_t* g_ids  = NULL;

sem_t* sem_empty = NULL;
sem_t* sem_full  = NULL;
sem_t* sem_mutex = NULL;
sem_t* sem_ids   = NULL;

static int map_create_fd(const char* name, size_t sz, int create) {
    int oflag = create ? (O_CREAT|O_RDWR) : O_RDWR;
    int fd = shm_open(name, oflag, 0666);
    if (fd < 0) return -1;
    if (create) {
        if (ftruncate(fd, (off_t)sz) < 0) { close(fd); return -1; }
    }
    return fd;
}

int ipc_open_all(int create, uint32_t total_ids) {
    // SHM ring
    int fd = map_create_fd(SHM_RING_NAME, sizeof(ring_t), create);
    if (fd < 0) return -1;
    g_ring = mmap(NULL, sizeof(ring_t), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (g_ring == MAP_FAILED) return -1;
    if (create) memset(g_ring, 0, sizeof(*g_ring));

    // SHM ids
    fd = map_create_fd(SHM_IDS_NAME, sizeof(ids_state_t), create);
    if (fd < 0) return -1;
    g_ids = mmap(NULL, sizeof(ids_state_t), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (g_ids == MAP_FAILED) return -1;
    if (create) {
        g_ids->next_id  = 1;
        g_ids->remaining = total_ids;
    }

    // Semáforos
    if (create) {
        sem_unlink(SEM_EMPTY_NAME);
        sem_unlink(SEM_FULL_NAME);
        sem_unlink(SEM_MUTEX_NAME);
        sem_unlink(SEM_IDS_NAME);
    }
    sem_empty = sem_open(SEM_EMPTY_NAME, create? O_CREAT:0, 0666, RING_CAP);
    sem_full  = sem_open(SEM_FULL_NAME , create? O_CREAT:0, 0666, 0);
    sem_mutex = sem_open(SEM_MUTEX_NAME, create? O_CREAT:0, 0666, 1);
    sem_ids   = sem_open(SEM_IDS_NAME  , create? O_CREAT:0, 0666, 1);
    if (!sem_empty || !sem_full || !sem_mutex || !sem_ids) return -1;

    return 0;
}

void ipc_close_all(int unlink_now) {
    if (g_ring) munmap(g_ring, sizeof(ring_t));
    if (g_ids)  munmap(g_ids,  sizeof(ids_state_t));

    if (sem_empty) sem_close(sem_empty);
    if (sem_full)  sem_close(sem_full);
    if (sem_mutex) sem_close(sem_mutex);
    if (sem_ids)   sem_close(sem_ids);

    if (unlink_now) {
        shm_unlink(SHM_RING_NAME);
        shm_unlink(SHM_IDS_NAME);
        sem_unlink(SEM_EMPTY_NAME);
        sem_unlink(SEM_FULL_NAME);
        sem_unlink(SEM_MUTEX_NAME);
        sem_unlink(SEM_IDS_NAME);
    }
}

void ring_push(const record_t* r) {
    sem_wait(sem_empty);
    sem_wait(sem_mutex);
    g_ring->buf[g_ring->tail] = *r;
    g_ring->tail = (g_ring->tail + 1) % RING_CAP;
    g_ring->count++;
    sem_post(sem_mutex);
    sem_post(sem_full);
}

int ring_pop(record_t* out) {
    // devuelve 0 si obtuvo un registro, -1 si no hay (debería coordinarse con 'remaining')
    if (sem_trywait(sem_full) != 0) return -1;
    sem_wait(sem_mutex);
    *out = g_ring->buf[g_ring->head];
    g_ring->head = (g_ring->head + 1) % RING_CAP;
    g_ring->count--;
    sem_post(sem_mutex);
    sem_post(sem_empty);
    return 0;
}

int ids_take_block(uint32_t* base, uint32_t* count) {
    // asigna hasta 10 IDs; devuelve 0 si asignó, 1 si ya no quedan.
    sem_wait(sem_ids);
    uint32_t give = (g_ids->remaining > 10) ? 10 : g_ids->remaining;
    if (give == 0) {
        sem_post(sem_ids);
        *base = 0; *count = 0;
        return 1; // no quedan
    }
    *base  = g_ids->next_id;
    *count = give;
    g_ids->next_id  += give;
    g_ids->remaining -= give;
    sem_post(sem_ids);
    return 0;
}
