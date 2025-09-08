// src/coordinator.c
// -----------------------------------------------------------------------------
// Propósito del archivo:
// - Implementa la rutina del COORDINADOR (consumidor).
// - Flujo:
//   1) Abre SHM y semáforos.
//   2) Abre el CSV, escribe encabezado (ID primero).
//   3) Extrae EXACTAMENTE 'total' registros del ring (ring_pop) y los vuelca
//      al CSV en el orden de arribo (no es necesario ordenar por ID).
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

    // Aseguramos punto decimal en el CSV
    setlocale(LC_NUMERIC, "C");

    FILE *f = fopen(csv_path, "w");
    if (!f) perr("fopen csv");

    // Encabezado: id,generador,pid,nombreProducto,precio,stock
    fprintf(f, "id,generador,pid,nombreProducto,precio,stock\n");

    for (int i = 0; i < total; ++i) {
        record_t r;
        ring_pop(shm, &sems, &r);
        // Pausa aleatoria de 10 a 50 milisegundos
        usleep(1000 * (10 + rand()%40));
        fprintf(f, "%d,%d,%d,%s,%.2f,%d\n",
                r.id, r.generador, (int)r.pid, r.nombreProducto, r.precio, r.stock);
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
