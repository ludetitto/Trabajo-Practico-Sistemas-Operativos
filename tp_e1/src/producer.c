// src/producer.c
// -----------------------------------------------------------------------------
// Proceso GENERADOR (productor):
//  - Abre SHM y semáforos por nombre.
//  - Pide bloques de IDs de a 10.
//  - Para cada ID: genera registro aleatorio y lo "empuja" a la cola (ring_push).
//  - Termina al agotarse los IDs.
// -----------------------------------------------------------------------------

#include "producer.h"
#include "ipc.h"
#include "ring.h"

void generator_loop(const names_t *nn) {
    // Abrimos SHM existente (lo creó el coordinador en main)
    int fd = shm_open(nn->shm_name, O_RDWR, 0600);
    if (fd == -1) perr("shm_open gen");

    // Obtenemos tamaño para mapear correctamente
    struct stat st;
    if (fstat(fd, &st) == -1) perr("fstat gen");

    shm_region_t *shm = (shm_region_t*) map_shm(fd, (size_t)st.st_size);
    sems_t sems = open_sems(nn);

    rand_seed();

    for (;;) {
        int start = 0, cnt = 0;

        // Solicita un bloque de IDs (atómico). Si no hay, terminamos.
        if (!request_id_block(shm, &sems, &start, &cnt)) {
            break; // no quedan IDs por generar
        }

        for (int i = 0; i < cnt; ++i) {
            record_t r;
            fill_random_record(&r, start + i);
            // Deposita UN registro por vez (requisito) en la cola
            ring_push(shm, &sems, &r);
        }
    }

    // Limpieza local (eliminación global la hace el padre)
    munmap(shm, (size_t)st.st_size);
    close(fd);
    sem_close(sems.empty);
    sem_close(sems.full);
    sem_close(sems.mutex);
    sem_close(sems.idlock);

    _exit(0); // salir del proceso hijo
}
