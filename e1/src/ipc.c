#include "../include/ipc.h"

// ===== SHM globales =====
cola_t *cola = NULL;
ids_t *ids_estado = NULL;

// ===== Semáforos globales =====
sem_t *sem_empty = NULL;
sem_t *sem_full = NULL;
sem_t *sem_mutex = NULL;
sem_t *sem_ids = NULL;

// Crea/abre un objeto SHM con tamaño 'tam'.
//   crear=1 => O_CREAT|O_RDWR + ftruncate; crear=0 => O_RDWR.
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

// Abre/crea todos los IPCs (cola + ids + semáforos)
int ipc_abrir_todos(int crear, uint32_t total_ids)
{
    // SHM cola (ring buffer)
    int fd = crear_shm(SHM_RING_NAME, sizeof(cola_t), crear);
    if (fd < 0)
        return -1;
    cola = mmap(NULL, sizeof(cola_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (cola == MAP_FAILED)
        return -1;
    if (crear)
        memset(cola, 0, sizeof(*cola));

    // SHM ids (estado de IDs + metadatos RR)
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
        ids_estado->nprods = 0; // lo setea el padre con ipc_set_children()
        ids_estado->turno = 0;
    }

    // Semáforos POSIX (unlink previos si creamos)
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

// Cierra y opcionalmente destruye todos los IPCs
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

// ===== Helpers de RR y estado de hijos =====

void ipc_set_children(int nprods, const pid_t *pids)
{
    if (!ids_estado)
        return;

    while (sem_wait(sem_ids) == -1 && errno == EINTR)
    { /* retry */
    }

    if (nprods > MAX_PRODS)
        nprods = MAX_PRODS;
    ids_estado->nprods = nprods;
    for (int i = 0; i < nprods; ++i)
    {
        ids_estado->pid[i] = pids[i];
        ids_estado->alive[i] = 1;
    }
    ids_estado->turno = 0;

    sem_post(sem_ids);
}

void ipc_mark_dead(pid_t pid)
{
    if (!ids_estado)
        return;

    while (sem_wait(sem_ids) == -1 && errno == EINTR)
    { /* retry */
    }

    for (int i = 0; i < ids_estado->nprods; ++i)
    {
        if (ids_estado->pid[i] == pid)
        {
            ids_estado->alive[i] = 0;
            // Si muere un hijo mientras aun quedan IDs por asignar,
            // marcamos que hubo una muerte prematura para que el
            // coordinador pueda decidir terminar (el total ya no se
            // podrá alcanzar).
            if (ids_estado->restantes > 0)
                ids_estado->muerte_temprana = 1;
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
        // Acá no reintentamos indefinidamente en EINTR, para que
        // el coordinador pueda reaccionar a señales.
    }
    int v = ids_estado->muerte_temprana ? 1 : 0;
    sem_post(sem_ids);
    return v;
}

uint32_t ipc_restantes(void)
{
    while (sem_wait(sem_ids) == -1 && errno == EINTR)
    { /* retry */
    }
    uint32_t r = ids_estado->restantes;
    sem_post(sem_ids);
    return r;
}

int ipc_prods_vivos(void)
{
    if (!ids_estado)
        return 0;

    while (sem_wait(sem_ids) == -1 && errno == EINTR)
    {
        // Acá no reintentamos indefinidamente en EINTR, para que
        // el coordinador pueda reaccionar a señales.
    }

    int vivos = 0;
    for (int i = 0; i < ids_estado->nprods; ++i)
        vivos += ids_estado->alive[i];

    sem_post(sem_ids);
    return vivos;
}

int ipc_nprods(void) // devuelve la cantidad de generadores publicados por el padre
{
    if (!ids_estado)
        return 0;

    while (sem_wait(sem_ids) == -1 && errno == EINTR)
    {
        // Acá no reintentamos indefinidamente en EINTR, para que
        // el coordinador pueda reaccionar a señales.
    }
    int n = ids_estado->nprods;
    sem_post(sem_ids);
    return n;
}

// ===== Ring buffer =====

void push(const registro_t *r)
{
    // Espera espacio libre y exclusión
    while (sem_wait(sem_empty) == -1 && errno == EINTR)
    { /* retry */
    }
    while (sem_wait(sem_mutex) == -1 && errno == EINTR)
    { /* retry */
    }

    // Escribe en la cola
    cola->buffer[cola->ultimo] = *r;
    cola->ultimo = (cola->ultimo + 1) % COLA_CAP;
    cola->cant_elem_ocupados++;

    // Libera exclusión y notifica que hay datos
    sem_post(sem_mutex);
    sem_post(sem_full);
}

int pop(registro_t *r)
{
    // Espera datos y exclusión
    while (sem_wait(sem_full) == -1 && errno == EINTR)
    { /* retry */
    }
    while (sem_wait(sem_mutex) == -1 && errno == EINTR)
    { /* retry */
    }

    // Lee desde la cola
    *r = cola->buffer[cola->primero];
    cola->primero = (cola->primero + 1) % COLA_CAP;
    cola->cant_elem_ocupados--;

    // Libera exclusión y notifica espacio libre
    sem_post(sem_mutex);
    sem_post(sem_empty);
    return 0;
}

int pop_timeout(registro_t *r, int timeout_ms)
{
    // Calcula deadline absoluto (CLOCK_REALTIME)
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

    // Espera con timeout por elementos
    // Retornos:
    //   0  -> éxito (se consumió un registro en la cola)
    //   1  -> timeout (no llegó nada dentro de timeout_ms)
    //  -1  -> error (incluye interrupción por señal/EINTR)
    // Nota: anteriormente se reintentaba en EINTR; aquí, para permitir que
    // las señales interrumpan la espera inmediatamente (y que el
    // coordinador pueda reaccionar vía handler), no reintentamos sobre
    // EINTR y consideramos -1 como error/interrupción.
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

    // Toma exclusión y consume
    while (sem_wait(sem_mutex) == -1 && errno == EINTR)
    { /* retry */
    }

    *r = cola->buffer[cola->primero];
    cola->primero = (cola->primero + 1) % COLA_CAP;
    cola->cant_elem_ocupados--;

    sem_post(sem_mutex);
    sem_post(sem_empty);
    return 0;
}

// ===== IDs: Round-Robin estricto (salta hijos muertos) =====
// idx = índice lógico del generador [0..nprods-1]
// return: 0=asignó; 1=no quedan IDs
int pedir_bloque_ids_rr(int idx, uint32_t *base, uint32_t *cant)
{
    for (;;)
    {
        // Toma lock del estado de IDs
        while (sem_wait(sem_ids) == -1 && errno == EINTR)
        { /* retry */
        }

        // ¿No quedan IDs?
        if (ids_estado->restantes == 0)
        {
            sem_post(sem_ids);
            *base = *cant = 0;
            return 1;
        }

        // Avanza turno si señala a un hijo muerto
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

            // Turno al siguiente vivo (si quedan IDs)
            if (ids_estado->nprods > 0)
            {
                do
                {
                    ids_estado->turno = (ids_estado->turno + 1) % ids_estado->nprods;
                } while (ids_estado->alive[ids_estado->turno] == 0 &&
                         ids_estado->restantes > 0);
            }

            sem_post(sem_ids);
            return 0;
        }

        // No es mi turno: suelta lock y espera breve
        sem_post(sem_ids);
        struct timespec ts = {.tv_sec = 0, .tv_nsec = 2000000L}; // 2ms
        nanosleep(&ts, NULL);
    }
}
