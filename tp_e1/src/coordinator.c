// src/coordinator.c
// -----------------------------------------------------------------------------
// COORDINADOR:
//  - Abre SHM y semáforos.
//  - Crea CSV y escribe encabezado (ID primero, luego prioridad, etc.).
//  - Extrae EXACTAMENTE 'total' registros del ring y los persiste en orden de arribo.
// -----------------------------------------------------------------------------

#include "coordinator.h"
#include "ipc.h"
#include "ring.h"

void coordinator_run(const names_t *nn, int total, const char *csv_path) {
    int fd = shm_open(nn->shm_name, O_RDWR, 0600);
    if (fd == -1) perr("shm_open coord");

    struct stat st;
    if (fstat(fd, &st) == -1) perr("fstat coord");

    shm_region_t *shm = (shm_region_t*) map_shm(fd, (size_t)st.st_size);
    sems_t sems = open_sems(nn);

    FILE *f = fopen(csv_path, "w");
    if (!f) perr("fopen csv");

    // Encabezado: ID primero (requisito), luego campos de la regla de negocio
    fprintf(f, "id,prioridad,nombreUser,evento,estado\n");

    for (int i = 0; i < total; ++i) {
        record_t r;
        ring_pop(shm, &sems, &r);
        fprintf(f, "%d,%d,%s,%s,%s\n",
                r.id, r.prioridad, r.nombreUser, r.evento, r.estado);
    }

    fflush(f);
    fclose(f);

    munmap(shm, (size_t)st.st_size);
    close(fd);
    sem_close(sems.empty);
    sem_close(sems.full);
    sem_close(sems.mutex);
    sem_close(sems.idlock);
}
