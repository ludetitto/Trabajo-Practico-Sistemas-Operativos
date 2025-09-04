// src/main.c
// -----------------------------------------------------------------------------
// Punto de entrada del Ejercicio 1 (productor-consumidor con SHM + semáforos).
//  - Parsea parámetros (-n generadores, -t total, -o CSV, -b buffer).
//  - Crea SHM + semáforos; inicializa ring y contadores (ID y PRIORIDAD).
//  - fork() de N generadores.
//  - El padre corre el coordinador y persiste EXACTAMENTE 'total'.
//  - Espera a hijos y hace limpieza total de IPC.
// -----------------------------------------------------------------------------

#include "common.h"
#include "ipc.h"
#include "ring.h"
#include "producer.h"
#include "coordinator.h"

static volatile sig_atomic_t g_stop = 0;
static void on_sigint(int sig) { (void)sig; g_stop = 1; }

static void usage(const char *prog) {
    fprintf(stderr,
        "Uso: %s -n <generadores> -t <total_registros> -o <salida.csv> [-b <capacidad_buffer>]\n"
        "Ej:  %s -n 4 -t 10000 -o datos.csv -b 128\n", prog, prog);
}

int main(int argc, char **argv) {
    int nprods = -1, total = -1, opt;
    size_t bufcap = DEFAULT_BUF_CAP;
    const char *out_csv = NULL;

    while ((opt = getopt(argc, argv, "n:t:o:b:h")) != -1) {
        switch (opt) {
            case 'n': nprods = atoi(optarg); break;
            case 't': total  = atoi(optarg); break;
            case 'o': out_csv= optarg;       break;
            case 'b': bufcap = (size_t)atoi(optarg); break;
            case 'h': default: usage(argv[0]); return (opt=='h')?0:1;
        }
    }

    if (nprods <= 0 || total <= 0 || !out_csv || bufcap < 2) {
        usage(argv[0]); return 1;
    }

    signal(SIGINT, on_sigint);
    rand_seed();

    names_t nn;
    gen_names(&nn);

    // Crear SHM con layout [cabecera + buffer flexible]
    size_t total_size = sizeof(shm_region_t) + sizeof(record_t) * bufcap;
    int fd = create_shm(nn.shm_name, total_size);

    shm_region_t *shm = (shm_region_t*) map_shm(fd, total_size);
    shm->capacity = bufcap;
    shm->head     = 0;
    shm->tail     = 0;
    shm->next_id  = 1;      // IDs 1..total
    shm->next_prio= 1;      // PRIORIDADES 1..total
    shm->total    = total;
    shm->stop     = 0;

    sems_t sems = create_sems(&nn, bufcap);

    // Lanzar N generadores
    pid_t *pids = calloc((size_t)nprods, sizeof(pid_t));
    if (!pids) die("calloc pids");

    for (int i = 0; i < nprods; ++i) {
        pid_t pid = fork();
        if (pid < 0) perr("fork");
        if (pid == 0) {
            generator_loop(&nn); // hijo
        } else {
            pids[i] = pid;       // padre
        }
    }

    // Padre => coordinador
    coordinator_run(&nn, total, out_csv);

    // Esperar hijos
    for (int i = 0; i < nprods; ++i) {
        int st = 0;
        (void)waitpid(pids[i], &st, 0);
    }

    // Limpieza TOTAL de IPC (padre)
    sem_close(sems.empty);
    sem_close(sems.full);
    sem_close(sems.mutex);
    sem_close(sems.idlock);
    unlink_all(&nn);

    munmap(shm, total_size);
    close(fd);
    free(pids);

    return 0;
}
