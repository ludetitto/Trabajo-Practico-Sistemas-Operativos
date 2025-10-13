#include "../include/ipc.h"

// ===== SHM globales =====
cola_t *cola = NULL;
ids_t *ids_estado = NULL;

// ===== Semáforos globales =====
sem_t *sem_empty = NULL;
sem_t *sem_full = NULL;
sem_t *sem_mutex = NULL;
sem_t *sem_ids = NULL;

static int crear_shm(const char *nombre, size_t tam, int crear)
{
    int oflag = crear ? (O_CREAT | O_RDWR) : O_RDWR;
    int fd = shm_open(nombre, oflag, 0666);
    if (fd < 0)
        return -1;
    if (crear && ftruncate(fd, (off_t)tam) < 0)
    {
        close(fd);
        return -1;
    }
    return fd;
}

// ===== Helpers internos (devq) — bajo sem_ids =====
static inline int devq_pop(uint32_t *base, uint32_t *cant)
{
    if (ids_estado->devq_len == 0)
        return 0;
    rango_t r = ids_estado->devq[ids_estado->devq_head];
    ids_estado->devq_head = (ids_estado->devq_head + 1) % DEVQ_CAP;
    ids_estado->devq_len--;
    *base = r.base;
    *cant = r.cant;
    return 1;
}

static inline void devq_push(uint32_t base, uint32_t cant)
{
    if (cant == 0)
        return;
    if (ids_estado->devq_len >= DEVQ_CAP)
    {
        // muy raro; reinyectamos al pool nuevo
        ids_estado->restantes += cant;
        return;
    }
    ids_estado->devq[ids_estado->devq_tail].base = base;
    ids_estado->devq[ids_estado->devq_tail].cant = cant;
    ids_estado->devq_tail = (ids_estado->devq_tail + 1) % DEVQ_CAP;
    ids_estado->devq_len++;
}

// ===== abrir/cerrar IPCs =====
int ipc_abrir_todos(int crear, uint32_t total_ids)
{
    int fd = crear_shm(SHM_RING_NAME, sizeof(cola_t), crear);
    if (fd < 0)
        return -1;
    cola = mmap(NULL, sizeof(cola_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (cola == MAP_FAILED)
        return -1;
    if (crear)
        memset(cola, 0, sizeof(*cola));

    fd = crear_shm(SHM_IDS_NAME, sizeof(ids_t), crear);
    if (fd < 0)
        return -1;
    ids_estado = mmap(NULL, sizeof(ids_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (ids_estado == MAP_FAILED)
        return -1;

    if (crear)
    {
        memset(ids_estado, 0, sizeof(*ids_estado));
        ids_estado->proximo = 1;
        ids_estado->restantes = total_ids;
        ids_estado->nprods = 0;
        ids_estado->turno = 0;
        ids_estado->devq_head = ids_estado->devq_tail = ids_estado->devq_len = 0;
    }

    if (crear)
    {
        sem_unlink(SEM_EMPTY_NAME);
        sem_unlink(SEM_FULL_NAME);
        sem_unlink(SEM_MUTEX_NAME);
        sem_unlink(SEM_IDS_NAME);
    }
    sem_empty = sem_open(SEM_EMPTY_NAME, crear ? O_CREAT : 0, 0666, COLA_CAP);
    sem_full = sem_open(SEM_FULL_NAME, crear ? O_CREAT : 0, 0666, 0);
    sem_mutex = sem_open(SEM_MUTEX_NAME, crear ? O_CREAT : 0, 0666, 1);
    sem_ids = sem_open(SEM_IDS_NAME, crear ? O_CREAT : 0, 0666, 1);
    if (!sem_empty || !sem_full || !sem_mutex || !sem_ids)
        return -1;

    return 0;
}

void ipc_cerrar_todos(int borrar_ahora)
{
    if (cola)
        munmap(cola, sizeof(cola_t));
    if (ids_estado)
        munmap(ids_estado, sizeof(ids_t));

    if (sem_empty)
        sem_close(sem_empty);
    if (sem_full)
        sem_close(sem_full);
    if (sem_mutex)
        sem_close(sem_mutex);
    if (sem_ids)
        sem_close(sem_ids);

    if (borrar_ahora)
    {
        shm_unlink(SHM_RING_NAME);
        shm_unlink(SHM_IDS_NAME);
        sem_unlink(SEM_EMPTY_NAME);
        sem_unlink(SEM_FULL_NAME);
        sem_unlink(SEM_MUTEX_NAME);
        sem_unlink(SEM_IDS_NAME);
    }
}

// ===== RR / estado hijos =====
void ipc_set_children(int nprods, const pid_t *pids)
{
    if (!ids_estado)
        return;
    while (sem_wait(sem_ids) == -1 && errno == EINTR)
    {
    }

    if (nprods > MAX_PRODS)
        nprods = MAX_PRODS;
    ids_estado->nprods = nprods;
    for (int i = 0; i < nprods; ++i)
    {
        ids_estado->pid[i] = pids[i];
        ids_estado->alive[i] = 1;
        ids_estado->bloq_activo[i] = 0;
        ids_estado->bloq_avance[i] = 0;
        ids_estado->bloq_cant[i] = 0;
        ids_estado->bloq_base[i] = 0;
    }
    ids_estado->turno = 0;

    sem_post(sem_ids);
}

void ipc_mark_dead(pid_t pid)
{
    if (!ids_estado)
        return;
    while (sem_wait(sem_ids) == -1 && errno == EINTR)
    {
    }

    for (int i = 0; i < ids_estado->nprods; ++i)
    {
        if (ids_estado->pid[i] == pid)
        {
            ids_estado->alive[i] = 0;

            if (ids_estado->bloq_activo[i])
            {
                uint32_t av = ids_estado->bloq_avance[i];
                uint32_t cnt = ids_estado->bloq_cant[i];
                if (av < cnt)
                {
                    uint32_t base_devol = ids_estado->bloq_base[i] + av;
                    uint32_t cant_devol = cnt - av;
                    devq_push(base_devol, cant_devol);
                }
                ids_estado->bloq_activo[i] = 0;
            }
            break;
        }
    }

    sem_post(sem_ids);
}

int ipc_hubo_muerte_prematura(void)
{
    if (!ids_estado)
        return 0;
    while (sem_wait(sem_ids) == -1 && errno == EINTR)
    {
    }

    // aproximación: si queda trabajo y hay menos vivos que nprods
    uint32_t rest = ids_estado->restantes;
    for (int i = 0, k = ids_estado->devq_len, p = ids_estado->devq_head; i < k; ++i)
    {
        rest += ids_estado->devq[p].cant;
        p = (p + 1) % DEVQ_CAP;
    }
    int n = ids_estado->nprods, vivos = 0;
    for (int i = 0; i < n; ++i)
        vivos += ids_estado->alive[i];

    int v = (rest > 0 && vivos < n) ? 1 : 0;
    sem_post(sem_ids);
    return v;
}

uint32_t ipc_restantes(void)
{
    while (sem_wait(sem_ids) == -1 && errno == EINTR)
    {
    }
    uint32_t r = ids_estado->restantes;
    sem_post(sem_ids);
    return r;
}

// NUEVO: suma “restantes” + todo lo que haya en la cola de devoluciones.
uint32_t ipc_pendientes_total(void)
{
    while (sem_wait(sem_ids) == -1 && errno == EINTR)
    {
    }
    uint32_t total = ids_estado->restantes;
    for (int i = 0, k = ids_estado->devq_len, p = ids_estado->devq_head; i < k; ++i)
    {
        total += ids_estado->devq[p].cant;
        p = (p + 1) % DEVQ_CAP;
    }
    sem_post(sem_ids);
    return total;
}

int ipc_prods_vivos(void)
{
    if (!ids_estado)
        return 0;
    while (sem_wait(sem_ids) == -1 && errno == EINTR)
    {
    }

    int vivos = 0;
    for (int i = 0; i < ids_estado->nprods; ++i)
        vivos += ids_estado->alive[i];

    sem_post(sem_ids);
    return vivos;
}

int ipc_nprods(void)
{
    if (!ids_estado)
        return 0;
    while (sem_wait(sem_ids) == -1 && errno == EINTR)
    {
    }
    int n = ids_estado->nprods;
    sem_post(sem_ids);
    return n;
}

void ipc_avance_bloque(int idx)
{
    if (!ids_estado)
        return;
    while (sem_wait(sem_ids) == -1 && errno == EINTR)
    {
    }

    if (idx >= 0 && idx < ids_estado->nprods && ids_estado->bloq_activo[idx])
    {
        if (ids_estado->bloq_avance[idx] < ids_estado->bloq_cant[idx])
        {
            ids_estado->bloq_avance[idx]++;
            if (ids_estado->bloq_avance[idx] >= ids_estado->bloq_cant[idx])
            {
                ids_estado->bloq_activo[idx] = 0;
            }
        }
    }

    sem_post(sem_ids);
}

// ===== Ring buffer (cola de registros) =====

void push(const registro_t *r)
{
    while (sem_wait(sem_empty) == -1 && errno == EINTR)
    {
    }
    while (sem_wait(sem_mutex) == -1 && errno == EINTR)
    {
    }

    cola->buffer[cola->ultimo] = *r;
    cola->ultimo = (cola->ultimo + 1) % COLA_CAP;
    cola->cant_elem_ocupados++;

    sem_post(sem_mutex);
    sem_post(sem_full);
}

int pop(registro_t *r)
{
    while (sem_wait(sem_full) == -1 && errno == EINTR)
    {
    }
    while (sem_wait(sem_mutex) == -1 && errno == EINTR)
    {
    }

    *r = cola->buffer[cola->primero];
    cola->primero = (cola->primero + 1) % COLA_CAP;
    cola->cant_elem_ocupados--;

    sem_post(sem_mutex);
    sem_post(sem_empty);
    return 0;
}

int pop_timeout(registro_t *r, int timeout_ms)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) < 0)
        return -1;

    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L)
    {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000L;
    }

    int rc;
    do
    {
        rc = sem_timedwait(sem_full, &ts);
    } while (rc == -1 && errno == EINTR);

    if (rc == -1)
    {
        if (errno == ETIMEDOUT)
            return 1;
        return -1;
    }

    while (sem_wait(sem_mutex) == -1 && errno == EINTR)
    {
    }

    *r = cola->buffer[cola->primero];
    cola->primero = (cola->primero + 1) % COLA_CAP;
    cola->cant_elem_ocupados--;

    sem_post(sem_mutex);
    sem_post(sem_empty);
    return 0;
}

// === Push interruptible (sale por timeout o señal) ===
// return: 0=ok, 1=timeout al esperar, -1=error
int push_interruptible(const registro_t *r, int timeout_ms)
{
    struct timespec ts1;
    if (clock_gettime(CLOCK_REALTIME, &ts1) < 0)
        return -1;
    ts1.tv_sec += timeout_ms / 1000;
    ts1.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
    if (ts1.tv_nsec >= 1000000000L)
    {
        ts1.tv_sec++;
        ts1.tv_nsec -= 1000000000L;
    }

    int rc;
    do
    {
        rc = sem_timedwait(sem_empty, &ts1);
    } while (rc == -1 && errno == EINTR);
    if (rc == -1)
    {
        if (errno == ETIMEDOUT)
            return 1;
        return -1;
    }

    struct timespec ts2;
    if (clock_gettime(CLOCK_REALTIME, &ts2) < 0)
    {
        sem_post(sem_empty);
        return -1;
    }
    ts2.tv_sec += timeout_ms / 1000;
    ts2.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
    if (ts2.tv_nsec >= 1000000000L)
    {
        ts2.tv_sec++;
        ts2.tv_nsec -= 1000000000L;
    }

    do
    {
        rc = sem_timedwait(sem_mutex, &ts2);
    } while (rc == -1 && errno == EINTR);
    if (rc == -1)
    {
        sem_post(sem_empty);
        if (errno == ETIMEDOUT)
            return 1;
        return -1;
    }

    cola->buffer[cola->ultimo] = *r;
    cola->ultimo = (cola->ultimo + 1) % COLA_CAP;
    cola->cant_elem_ocupados++;

    sem_post(sem_mutex);
    sem_post(sem_full);
    return 0;
}

// ===== IDs: RR con “devoluciones” =====
// idx = índice lógico del generador [0..nprods-1]
// return: 0=asignó; 1=no quedan IDs
int pedir_bloque_ids_rr(int idx, uint32_t *base, uint32_t *cant)
{
    for (;;)
    {
        while (sem_wait(sem_ids) == -1 && errno == EINTR)
        {
        }

        // ¿Hay un rango devuelto pendiente? -> Prioridad 1
        uint32_t b, c;
        if (ids_estado->devq_len > 0)
        {
            devq_pop(&b, &c);
            uint32_t give = (c > 10) ? 10 : c;
            *base = b;
            *cant = give;

            // Si sobró, reinsertamos el resto al frente (simplemente push otra vez)
            if (c > give)
            {
                devq_push(b + give, c - give);
            }

            // Registrar bloque activo del generador
            ids_estado->bloq_base[idx] = *base;
            ids_estado->bloq_cant[idx] = *cant;
            ids_estado->bloq_avance[idx] = 0;
            ids_estado->bloq_activo[idx] = 1;

            sem_post(sem_ids);
            return 0;
        }

        // ¿No quedan IDs “nuevos”?
        if (ids_estado->restantes == 0)
        {
            sem_post(sem_ids);
            *base = *cant = 0;
            return 1;
        }

        // Avanza turno si el apuntado está muerto
        int giros = 0;
        while (ids_estado->nprods > 0 &&
               ids_estado->alive[ids_estado->turno] == 0 &&
               giros < ids_estado->nprods)
        {
            ids_estado->turno = (ids_estado->turno + 1) % ids_estado->nprods;
            giros++;
        }

        // ¿Es mi turno y estoy vivo?
        if (ids_estado->nprods > 0 &&
            ids_estado->turno == idx &&
            ids_estado->alive[idx])
        {

            uint32_t give = (ids_estado->restantes > 10) ? 10 : ids_estado->restantes;
            *base = ids_estado->proximo;
            *cant = give;

            ids_estado->proximo += give;
            ids_estado->restantes -= give;

            // Siguiente turno (saltando muertos)
            if (ids_estado->nprods > 0)
            {
                do
                {
                    ids_estado->turno = (ids_estado->turno + 1) % ids_estado->nprods;
                } while (ids_estado->alive[ids_estado->turno] == 0 &&
                         ids_estado->restantes > 0);
            }

            // Registrar bloque activo para este generador
            ids_estado->bloq_base[idx] = *base;
            ids_estado->bloq_cant[idx] = *cant;
            ids_estado->bloq_avance[idx] = 0;
            ids_estado->bloq_activo[idx] = 1;

            sem_post(sem_ids);
            return 0;
        }

        // No es mi turno: suelta lock y espera breve
        sem_post(sem_ids);
        struct timespec ts = {.tv_sec = 0, .tv_nsec = 2000000L}; // 2ms
        nanosleep(&ts, NULL);
    }
}
