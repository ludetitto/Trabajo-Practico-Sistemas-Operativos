// src/producer.c
// -----------------------------------------------------------------------------
// Propósito del archivo:
// - Implementa la rutina del proceso GENERADOR.
// - Flujo:
//   1) Abre SHM y semáforos (creados por el padre).
//   2) Pide bloques de IDs (10 en 10).
//   3) Para cada ID del bloque: completa un record_t con
//      * id asignado,
//      * generador = índice lógico 0..N-1,
//      * pid = getpid(),
//      * campos de producto (nombre, precio, stock),
//      y lo envía de a UNO al ring con ring_push().
//   4) Termina cuando no hay más IDs.
// -----------------------------------------------------------------------------

#include "generator.h"
#include "ipc.h"
#include "ring.h"

void generator_loop(const names_t *nn, int generador_index) {
    int fd = shm_open(nn->shm_name, O_RDWR, 0600);
    if (fd == -1) perr("shm_open gen");

    struct stat st;
    if (fstat(fd, &st) == -1) perr("fstat gen");

    shm_region_t *shm = (shm_region_t*) map_shm(fd, (size_t)st.st_size);
    sems_t sems = open_sems(nn);

    rand_seed();

    for (;;) {
        int start = 0, cnt = 0;

        // Reservar próximo bloque de IDs (atómico)
        if (!request_id_block(shm, &sems, &start, &cnt)) {
            break; // no quedan más IDs
        }

        for (int i = 0; i < cnt; ++i) {
            record_t r;
            r.id        = start + i;
            r.generador = generador_index;
            r.pid       = getpid();

            fill_random_product_fields(&r);  // nombre, precio, stock
            ring_push(shm, &sems, &r);       // enviar UN registro por vez
            usleep(1000 * (10 + rand()%40)); // 10..49 ms (solo para debug)
            fprintf(stderr, "[G%d pid=%d] push id=%d\n", generador_index,(int)getpid(), r.id);

        }
    }

    // Limpieza local (eliminación global la hace el padre)
    munmap(shm, (size_t)st.st_size);
    close(fd);
    sem_close(sems.empty);
    sem_close(sems.full);
    sem_close(sems.mutex);
    sem_close(sems.idlock);

    _exit(0);
}
