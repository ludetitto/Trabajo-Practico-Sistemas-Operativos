#define _POSIX_C_SOURCE 200809L
#include "comun.h"
#include <unistd.h>


/* Helper: formatea timestamp con milisegundos (opcional; usamos segundos para simplicidad) */
static void now_str(char *buf, size_t sz) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm;
    localtime_r(&ts.tv_sec, &tm);
    int ms = ts.tv_nsec / 1000000;
    /* YYYY-MM-DD HH:MM:SS.mmm */
    snprintf(buf, sz, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec, ms);
}

/* Worker: código que ejecutan los hijos (generadores).
   Cada trabajador toma bloques de BLOCK_SIZE IDs usando sem_id,
   genera cada producto y lo encola 1 a 1 protegiendo cola con semáforos. */
static void generator_worker(int total) {
    /* Abrir la SHM mapeada (ya creada por padre) */
    ShmQueue *queue = open_and_map_shm(SHM_NAME, sizeof(ShmQueue));
    if (!queue) {
        fprintf(stderr, "Generador %d: no encuentra SHM. Salir.\n", getpid());
        _exit(1);
    }

    sem_t *sem_empty = sem_open(SEM_EMPTY_NAME, 0);
    sem_t *sem_full  = sem_open(SEM_FULL_NAME, 0);
    sem_t *sem_mutex = sem_open(SEM_MUTEX_NAME, 0);
    sem_t *sem_id    = sem_open(SEM_ID_NAME, 0);

    if (sem_empty == SEM_FAILED || sem_full == SEM_FAILED ||
        sem_mutex == SEM_FAILED || sem_id == SEM_FAILED) {
        fprintf(stderr, "Generador %d: sem_open error\n", getpid());
        _exit(1);
    }

    srand(time(NULL) ^ (getpid()<<16));

    int produced = 0;
    while (produced < total) {
        /* conseguir bloque de IDs */
        int take = 0;
        int ids[BLOCK_SIZE];

        if (sem_wait(sem_id) < 0) {
            if (errno == EINTR) continue;
            perror("sem_wait sem_id");
            break;
        }

        /* calcular cuantos quedan disponibles globalmente */
        int next = queue->next_id;
        int remaining = queue->total_T - next + 1;
        if (remaining <= 0) {
            /* no quedan */
            sem_post(sem_id);
            break;
        }
        take = (remaining >= BLOCK_SIZE) ? BLOCK_SIZE : remaining;
        for (int i = 0; i < take; ++i) ids[i] = next + i;
        queue->next_id = next + take;
        sem_post(sem_id);

        /* generar y encolar cada id individualmente */
        for (int i = 0; i < take && produced < total; ++i) {
            Product p;
            p.id = ids[i];
            snprintf(p.name, sizeof(p.name), "Producto%d", p.id);
            p.price = (rand() % 10000) / 100.0; /* 0.00..99.99 */
            p.stock = rand() % 200;
            p.generator_pid = getpid();
            now_str(p.ts, sizeof(p.ts));

            /* productor: esperar empty, tomar mutex, push, post mutex, post full */
            if (sem_wait(sem_empty) < 0) {
                if (errno == EINTR) { i--; continue; }
                perror("sem_wait empty");
                goto cleanup;
            }
            if (sem_wait(sem_mutex) < 0) {
                perror("sem_wait mutex");
                /* intentar liberar empty para no perder estado */
                sem_post(sem_empty);
                goto cleanup;
            }

            /* push */
            queue->buf[queue->tail] = p;
            queue->tail = (queue->tail + 1) % QUEUE_CAP;
            queue->count++;

            if (sem_post(sem_mutex) < 0) { perror("sem_post mutex"); }
            if (sem_post(sem_full) < 0) { perror("sem_post full"); }

            /* imprimir para debugging/concurrencia */
            printf("Generador %d produjo ID=%d\n", getpid(), p.id);
            fflush(stdout);

            produced++;
            /* añadir una pequeña espera aleatoria para favorecer interleaving */
            struct timespec ts;
            ts.tv_sec = 0;
            ts.tv_nsec = (rand() % 50000) * 1000; /* microsegundos → nanosegundos */
            nanosleep(&ts, NULL);

        }
    }

cleanup:
    sem_close(sem_empty); sem_close(sem_full);
    sem_close(sem_mutex); sem_close(sem_id);
    munmap(queue, sizeof(ShmQueue));
    _exit(0);
}

/* Main: coordinador que crea IPC, forkea N generadores y consume la cola hasta T registros,
   guardando en CSV. */
int main(int argc, char *argv[]) {
    int N = 0;
    int T = 0;
    char *filename = NULL;
    int opt;

    while ((opt = getopt(argc, argv, "n:t:f:")) != -1) {
        switch (opt) {
            case 'n': N = atoi(optarg); break;
            case 't': T = atoi(optarg); break;
            case 'f':
                filename = strdup(optarg);
                break;
            default:
                fprintf(stderr, "Uso: %s -n <generadores> -t <total> -f <salida.csv>\n", argv[0]);
                return 1;
        }
    }
    if (N <= 0 || T <= 0 || filename == NULL) {
        fprintf(stderr, "Uso: %s -n <generadores> -t <total> -f <salida.csv>\n", argv[0]);
        return 1;
    }

    /* limpiar posibles restos IPC de ejecuciones previas */
    shm_unlink(SHM_NAME);
    sem_unlink(SEM_EMPTY_NAME); sem_unlink(SEM_FULL_NAME);
    sem_unlink(SEM_MUTEX_NAME); sem_unlink(SEM_ID_NAME);

    /* crear y mapear SHM */
    ShmQueue *queue = create_and_map_shm(SHM_NAME, sizeof(ShmQueue));
    if (!queue) perror_exit("create_and_map_shm");

    /* inicializar la cola y contador global */
    queue->head = queue->tail = queue->count = 0;
    queue->next_id = 1;
    queue->total_T = T;

    /* crear semáforos nombrados */
    sem_t *sem_empty = sem_open(SEM_EMPTY_NAME, O_CREAT | O_EXCL, 0600, QUEUE_CAP);
    sem_t *sem_full  = sem_open(SEM_FULL_NAME,  O_CREAT | O_EXCL, 0600, 0);
    sem_t *sem_mutex = sem_open(SEM_MUTEX_NAME, O_CREAT | O_EXCL, 0600, 1);
    sem_t *sem_id    = sem_open(SEM_ID_NAME,    O_CREAT | O_EXCL, 0600, 1);
    if (sem_empty == SEM_FAILED || sem_full == SEM_FAILED ||
        sem_mutex == SEM_FAILED || sem_id == SEM_FAILED) {
        perror("sem_open create");
        unlink_ipc();
        return 1;
    }

    /* cerramos handles locales: sems permanecen en el sistema */
    sem_close(sem_empty); sem_close(sem_full);
    sem_close(sem_mutex); sem_close(sem_id);

    printf("Coordinador: %d generadores, total=%d, archivo=%s\n", N, T, filename);

    /* fork N generadores */
    for (int i = 0; i < N; ++i) {
        pid_t pid = fork();
        if (pid < 0) perror_exit("fork");
        if (pid == 0) {
            /* proceso hijo: ejecuta worker; cada hijo recibe como 'total' el T
               (puede generar hasta T, el sem_id controla que no se sobrepase) */
            generator_worker(T);
            /* never returns */
        }
        /* padre continúa */
    }

    /* Reabrir semáforos para consumir */
    sem_empty = sem_open(SEM_EMPTY_NAME, 0);
    sem_full  = sem_open(SEM_FULL_NAME, 0);
    sem_mutex = sem_open(SEM_MUTEX_NAME, 0);
    if (sem_empty == SEM_FAILED || sem_full == SEM_FAILED || sem_mutex == SEM_FAILED)
        perror_exit("sem_open parent");

    /* abrir archivo CSV */
    FILE *f = fopen(filename, "w");
    if (!f) perror_exit("fopen CSV");
    fprintf(f, "ID,NombreProducto,Precio,Stock,GeneradorPID,Timestamp\n");
    fflush(f);

    /* consumir hasta T registros */
    int written = 0;
    while (written < T) {
        if (sem_wait(sem_full) < 0) {
            if (errno == EINTR) continue;
            perror_exit("sem_wait full parent");
        }
        if (sem_wait(sem_mutex) < 0) perror_exit("sem_wait mutex parent");

        Product p = queue->buf[queue->head];
        queue->head = (queue->head + 1) % QUEUE_CAP;
        queue->count--;

        if (sem_post(sem_mutex) < 0) perror_exit("sem_post mutex parent");
        if (sem_post(sem_empty) < 0) perror_exit("sem_post empty parent");

        /* escribir en CSV */
        fprintf(f, "%d,%s,%.2f,%d,%d,%s\n",
                p.id, p.name, p.price, p.stock, (int)p.generator_pid, p.ts);
        fflush(f);
        written++;
    }

    printf("Coordinador: escribió %d registros. Limpiando...\n", written);
    fclose(f);

    /* esperar hijos */
    for (int i = 0; i < N; ++i) wait(NULL);

    /* cleanup IPC */
    unlink_ipc();
    munmap(queue, sizeof(ShmQueue));

    free(filename);
    printf("Coordinador finalizado. Archivo: salida.csv\n");
    return 0;
}















