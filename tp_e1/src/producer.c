// src/producer.c
// -----------------------------------------------------------------------------
// GENERADOR:
//  - Abre SHM y semáforos por nombre.
//  - Solicita bloques de IDs y PRIORIDADES de a 10 (últimos pueden ser <10).
//  - Pareamos ID<->PRIORIDAD en el mismo orden para garantizar 1..N contiguo.
//  - Para cada par, generamos un registro y lo empujamos al ring (uno por vez).
// -----------------------------------------------------------------------------

#include "producer.h"
#include "ipc.h"
#include "ring.h"

void generator_loop(const names_t *nn) {
    int fd = shm_open(nn->shm_name, O_RDWR, 0600);
    if (fd == -1) perr("shm_open gen");

    struct stat st;
    if (fstat(fd, &st) == -1) perr("fstat gen");

    shm_region_t *shm = (shm_region_t*) map_shm(fd, (size_t)st.st_size);
    sems_t sems = open_sems(nn);

    rand_seed();

    for (;;) {
        int id_start=0, id_cnt=0;
        int pr_start=0, pr_cnt=0;

        bool have_ids  = request_id_block  (shm, &sems, &id_start, &id_cnt);
        bool have_prio = request_prio_block(shm, &sems, &pr_start, &pr_cnt);

        if (!have_ids || !have_prio) {
            break; // no quedan más elementos a producir
        }

        int cnt = (id_cnt < pr_cnt) ? id_cnt : pr_cnt;

        for (int i = 0; i < cnt; ++i) {
            record_t r;
            int id = id_start + i;
            int pr = pr_start + i;

            // Si querés snapshot con el primero marcado "Atendido", poné true.
            bool mark_first_attended = false;

            fill_random_record(&r, id, pr, mark_first_attended);
            ring_push(shm, &sems, &r);
        }
    }

    munmap(shm, (size_t)st.st_size);
    close(fd);
    sem_close(sems.empty);
    sem_close(sems.full);
    sem_close(sems.mutex);
    sem_close(sems.idlock);

    _exit(0);
}
