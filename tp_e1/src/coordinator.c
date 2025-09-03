// src/coordinator.c
// -----------------------------------------------------------------------------
// Proceso COORDINADOR (consumidor):
//  - Abre SHM y semáforos por nombre.
//  - Abre el CSV y escribe encabezado (ID como primera columna).
//  - Extrae EXACTAMENTE 'total' registros del ring (ring_pop) y los vuelca al CSV.
// -----------------------------------------------------------------------------

#include "coordinator.h"
#include "ipc.h"
#include "ring.h"

void coordinator_run(const names_t *nn, int total, const char *csv_path) {
    // Abrimos SHM creado por main
    int fd = shm_open(nn->shm_name, O_RDWR, 0600);
    if (fd == -1) perr("shm_open coord");

    struct stat st;
    if (fstat(fd, &st) == -1) perr("fstat coord");

    shm_region_t *shm = (shm_region_t*) map_shm(fd, (size_t)st.st_size);
    sems_t sems = open_sems(nn);

    // Abrimos CSV, escribimos encabezado requerido (ID primero)
    FILE *f = fopen(csv_path, "w");
    if (!f) perr("fopen csv");
    fprintf(f, "id,age,score,name,city\n");

    // Consumimos EXACTAMENTE 'total' registros y los persistimos
    for (int i = 0; i < total; ++i) {
        record_t r;
        ring_pop(shm, &sems, &r);
        // Se guarda en el orden de ARRIBO (no hace falta ordenar por id)
        fprintf(f, "%d,%d,%d,%s,%s\n", r.id, r.age, r.score, r.name, r.city);
    }

    fflush(f);
    fclose(f);

    // Limpieza local (eliminación global la hace el padre)
    munmap(shm, (size_t)st.st_size);
    close(fd);
    sem_close(sems.empty);
    sem_close(sems.full);
    sem_close(sems.mutex);
    sem_close(sems.idlock);
}
