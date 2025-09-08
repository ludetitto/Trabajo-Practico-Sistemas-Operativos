// src/main.c
// -----------------------------------------------------------------------------
// Propósito del archivo:
// - Punto de entrada. Orquesta la ejecución completa del Ejercicio 1:
//   * Parsea CLI: -n <generadores> -t <total> -o <csv> [-b <capacidad_buffer>]
//   * Crea SHM + semáforos e inicializa la cabecera de la región.
//   * Crea N procesos generadores (fork), pasando índice 0..N-1.
//   * En el padre, ejecuta el coordinador: consume EXACTAMENTE 'total' y persiste.
//   * Espera a los hijos y limpia TODOS los recursos IPC (sem_unlink/shm_unlink).
// -----------------------------------------------------------------------------

#include "common.h"
#include "ipc.h"
#include "ring.h"
#include "generator.h"
#include "coordinator.h"

static volatile sig_atomic_t g_stop = 0;
static void on_sigint(int sig) { (void)sig; g_stop = 1; }

static void usage(const char *prog) {
    fprintf(stderr,
        "Uso: %s -n <generadores> -t <total_registros> -o <salida.csv> [-b <capacidad_buffer>]\n"
        "Ej:  %s -n 4 -t 10000 -o productos.csv -b 128\n",
        prog, prog);
}

int main(int argc, char **argv) {
    int nprods = -1, total = -1, opt;
    size_t bufcap = DEFAULT_BUF_CAP;
    const char *out_csv = NULL;

    while ((opt = getopt(argc, argv, "n:t:o:b:h:p")) != -1) {
        switch (opt) {
            case 'n': nprods = atoi(optarg); break;
            case 't': total  = atoi(optarg); break;
            case 'o': out_csv= optarg;       break;
            case 'b': bufcap = (size_t)atoi(optarg); break;
            case 'p': print_help_examples(argv[0]); return 0;
            case 'h': default: usage(argv[0]); return (opt=='h')?0:1;
        }
    }

    // Validaciones mínimas (criterio de corrección típico)
    if (nprods <= 0 || total <= 0 || !out_csv || bufcap < 2) {
        usage(argv[0]); return 1;
    }

    signal(SIGINT, on_sigint);
    rand_seed();

    // Nombres POSIX únicos para SHM y semáforos
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
    shm->total    = total;
    shm->stop     = 0;

    // Crear semáforos
    sems_t sems = create_sems(&nn, bufcap);

    // Lanzar N generadores (índice lógico 0..N-1)
    pid_t *pids = calloc((size_t)nprods, sizeof(pid_t));
    if (!pids) die("calloc pids");

    for (int i = 0; i < nprods; ++i) {
        pid_t pid = fork();
        if (pid < 0) perr("fork");
        if (pid == 0) {
            generator_loop(&nn, i); // hijo => generador i
        } else {
            pids[i] = pid;          // padre guarda PID
        }
    }

    // Padre => coordinador (consume EXACTAMENTE 'total' y persiste CSV)
    coordinator_run(&nn, total, out_csv);

    // Esperar a los hijos
    for (int i = 0; i < nprods; ++i) {
        int st = 0;
        (void)waitpid(pids[i], &st, 0);
    }

    // Limpieza total de IPC (padre)
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
