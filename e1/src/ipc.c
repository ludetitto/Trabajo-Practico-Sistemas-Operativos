#include "../include/ipc.h"

// Memoria compartida
// globales para no tener que pasarlos como parámetro
// que son punteros a la memoria mapeada
cola_t *cola = NULL;      // buffer circular, sirve para pasar registros entre procesos
ids_t *ids_estado = NULL; // estado de los IDs, sirve para asignar IDs únicos

// Semáforos
// globales para no tener que pasarlos como parámetro
sem_t *sem_empty = NULL; // cuenta espacios vacíos en el buffer circular
sem_t *sem_full = NULL;  // cuenta elementos ocupados en el buffer circular
sem_t *sem_mutex = NULL; // mutex para acceso exclusivo al buffer circular
sem_t *sem_ids = NULL;   // mutex para acceso exclusivo al estado de los IDs

static int crear_shm(const char *nombre, size_t tam, int crear)
{                                                    // se traduce como mapear y crear si es necesario
    int oflag = crear ? (O_CREAT | O_RDWR) : O_RDWR; // oflag significa "open flag"
    int fd = shm_open(nombre, oflag, 0666);          // permisos rw-rw-rw de la biblioteca
    // shm es como un archivo, pero en memoria compartida
    // sirve para compartir memoria entre procesos
    if (fd < 0)
        return -1;
    if (crear)
    {
        if (ftruncate(fd, (off_t)tam) < 0)
        {
            close(fd);
            return -1;
        } // darle tamaño
    }
    return fd;
}

int ipc_abrir_todos(int crear, uint32_t total_ids)
{ // crear =1 para crear, 0 para abrir
    // SHM cola
    int fd = crear_shm(SHM_RING_NAME, sizeof(cola_t), crear);
    if (fd < 0)
        return -1;
    cola = mmap(NULL, sizeof(cola_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); // mapear en memoria
    close(fd);
    if (cola == MAP_FAILED)
        return -1;
    if (crear)
        memset(cola, 0, sizeof(*cola)); // inicializar en cero

    // SHM ids
    fd = crear_shm(SHM_IDS_NAME, sizeof(ids_t), crear);
    if (fd < 0)
        return -1;
    ids_estado = mmap(NULL, sizeof(ids_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (ids_estado == MAP_FAILED)
        return -1;
    if (crear)
    {
        ids_estado->proximo = 1;
        ids_estado->restantes = total_ids; // cantidad total de IDs que se van a pedir
    }

    // Semáforos
    if (crear)
    {
        sem_unlink(SEM_EMPTY_NAME); // eliminar si ya existían
        sem_unlink(SEM_FULL_NAME);
        sem_unlink(SEM_MUTEX_NAME);
        sem_unlink(SEM_IDS_NAME);
    }
    sem_empty = sem_open(SEM_EMPTY_NAME, crear ? O_CREAT : 0, 0666, COLA_CAP); // espacios vacíos = capacidad del buffer
    sem_full = sem_open(SEM_FULL_NAME, crear ? O_CREAT : 0, 0666, 0);          // elementos ocupados = 0
    sem_mutex = sem_open(SEM_MUTEX_NAME, crear ? O_CREAT : 0, 0666, 1);        // mutex binario
    sem_ids = sem_open(SEM_IDS_NAME, crear ? O_CREAT : 0, 0666, 1);            // mutex binario
    if (!sem_empty || !sem_full || !sem_mutex || !sem_ids)
        return -1;

    return 0;
}

void ipc_cerrar_todos(int borrar_ahora)
{
    if (cola)
        munmap(cola, sizeof(cola_t)); // liberar memoria mapeada
    if (ids_estado)
        munmap(ids_estado, sizeof(ids_t));

    if (sem_empty)
        sem_close(sem_empty); // liberar semáforos
    if (sem_full)
        sem_close(sem_full);
    if (sem_mutex)
        sem_close(sem_mutex);
    if (sem_ids)
        sem_close(sem_ids);

    if (borrar_ahora)
    {
        shm_unlink(SHM_RING_NAME); // eliminar memoria compartida
        shm_unlink(SHM_IDS_NAME);
        sem_unlink(SEM_EMPTY_NAME);
        sem_unlink(SEM_FULL_NAME);
        sem_unlink(SEM_MUTEX_NAME);
        sem_unlink(SEM_IDS_NAME);
    }
}

void push(const registro_t *r)
{ // bloquea hasta que haya espacio
    // pone un registro en el buffer circular

    sem_wait(sem_empty); // P --> hay al menos un espacio vacío
    sem_wait(sem_mutex); // P --> acceso exclusivo

    cola->buffer[cola->ultimo] = *r;              // copia el registro
    cola->ultimo = (cola->ultimo + 1) % COLA_CAP; // avanza el índice circular
    cola->cant_elem_ocupados++;                   // un elemento más

    sem_post(sem_mutex); // V --> acceso exclusivo terminado
    sem_post(sem_full);  // V --> hay un elemento más
}

/// COORDINACIÓN CLÁSICA PRODUCTOR-CONSUMIDOR
// Push / Pop con semáforos: Uno produce y muchos consumen.
int pop(registro_t *r)
{
    // Espera hasta que haya al menos un elemento
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

int pedir_bloque_ids(uint32_t *base, uint32_t *cant)
{
    uint32_t ids_asignados;
    // asigna hasta 10 IDs; devuelve 0 si asignó, 1 si ya no quedan.
    sem_wait(sem_ids); // P
    // cuántos doy? hasta 10, o los que queden si son menos
    ids_asignados = (ids_estado->restantes > 10) ? 10 : ids_estado->restantes;
    if (ids_asignados == 0)
    {
        sem_post(sem_ids); // V
        *base = 0;
        *cant = 0;
        return 1; // no quedan
    }
    *base = ids_estado->proximo; // primer ID a entregar
    *cant = ids_asignados;       // cuántos IDs entrego
    ids_estado->proximo += ids_asignados;
    ids_estado->restantes -= ids_asignados;
    sem_post(sem_ids); // V
    return 0;
}
