#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <getopt.h>

#define BLOQUE 10
#define BUFFER_SIZE 10
#define MAX_NAME 32

/* Nombres con sufijo para evitar choques entre corridas */
#define SHM_CONTROL   "/shm_control_ej1"
#define SHM_COLA      "/shm_cola_ej1"
#define SEM_MUTEX_ID  "/sem_mutex_id_ej1"
#define SEM_MUTEX_BUF "/sem_mutex_buf_ej1"
#define SEM_EMPTY     "/sem_empty_ej1"
#define SEM_FULL      "/sem_full_ej1"

typedef struct {
    int id;
    int gen;                 // NUEVO: número de generador (0..N-1)
    pid_t pid;               // NUEVO: PID del proceso generador
    char name[MAX_NAME];
} registro;

typedef struct {
    registro buffer[BUFFER_SIZE];
    int head;
    int tail;
} cola_shm;

typedef struct {
    int next_id;
    int max_id;
} control_shm;

/* ---- Globals para cleanup garantizado ---- */
static int fd_ctrl = -1, fd_cola = -1;
static control_shm *ctrl = NULL;
static cola_shm *cola = NULL;
static sem_t *mutex_id = NULL, *mutex_buffer = NULL, *sem_empty = NULL, *sem_full = NULL;
static volatile sig_atomic_t stop_requested = 0;

/* ---- Señales ---- */
static void on_signal(int s){ (void)s; stop_requested = 1; }

/* ---- Limpieza ---- */
static void cleanup_all(void){
    if (mutex_id && mutex_id != SEM_FAILED)        { sem_close(mutex_id);     sem_unlink(SEM_MUTEX_ID);  mutex_id = NULL; }
    if (mutex_buffer && mutex_buffer != SEM_FAILED){ sem_close(mutex_buffer); sem_unlink(SEM_MUTEX_BUF); mutex_buffer = NULL; }
    if (sem_empty && sem_empty != SEM_FAILED)      { sem_close(sem_empty);    sem_unlink(SEM_EMPTY);     sem_empty = NULL; }
    if (sem_full && sem_full != SEM_FAILED)        { sem_close(sem_full);     sem_unlink(SEM_FULL);      sem_full = NULL; }
    if (ctrl)         { munmap(ctrl, sizeof(*ctrl)); ctrl = NULL; }
    if (cola)         { munmap(cola, sizeof(*cola)); cola = NULL; }
    if (fd_ctrl!=-1)  { close(fd_ctrl); shm_unlink(SHM_CONTROL); fd_ctrl = -1; }
    if (fd_cola!=-1)  { close(fd_cola); shm_unlink(SHM_COLA);    fd_cola = -1; }
}

/* ---- Helper timeout para semáforos (evita cuelgue si muere coordinador/padre) ---- */
static int wait_with_timeout(sem_t* s, int timeout_ms){
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts)!=0) return -1;
    ts.tv_sec  += timeout_ms/1000;
    ts.tv_nsec += (long)(timeout_ms%1000)*1000000L;
    if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
    for(;;){
        if (sem_timedwait(s,&ts)==0) return 0;
        if (errno==EINTR) continue;
        if (errno==ETIMEDOUT) return 1;
        return -1;
    }
}

/* ---- Sleep en ms con nanosleep (sin usleep) ---- */
static void sleep_ms(int ms){
    if (ms <= 0) return;
    struct timespec req = { ms/1000, (long)(ms%1000)*1000000L }, rem;
    while (nanosleep(&req, &rem) == -1 && errno == EINTR) req = rem;
}

/* ---- Nombres aleatorios ---- */
static void generar_nombre(char *name) {
    static const char *nombres[] = {"Ana","Juan","Luis","Marta","Pedro","Sofia","Diego","Clara"};
    int idx = rand() % 8;
    snprintf(name, MAX_NAME, "%s", nombres[idx]);  // seguro y sin warning
}

/* ---- Ayuda CLI ---- */
static void print_usage(const char* p){
    fprintf(stderr,
      "Uso: %s -n <generadores> -t <total> -f <csv> [-d <delay_ms>]\n"
      "  -n: cantidad de generadores (hijos)\n"
      "  -t: total de registros a generar\n"
      "  -f: archivo CSV de salida\n"
      "  -d: delay artificial por registro en ms (default 0)\n", p);
}

int main(int argc, char *argv[]) {
    int N=-1, TOTAL=-1, delay_ms=0;
    const char* CSV = "registros.csv";

    int opt;
    while((opt=getopt(argc,argv,"n:t:f:d:h"))!=-1){
        switch(opt){
            case 'n': N = atoi(optarg); break;
            case 't': TOTAL = atoi(optarg); break;
            case 'f': CSV = optarg; break;
            case 'd': delay_ms = atoi(optarg); break;
            case 'h': default: print_usage(argv[0]); return 1;
        }
    }
    if (N<=0 || TOTAL<=0 || !CSV || !*CSV){ print_usage(argv[0]); return 1; }

    /* Señales + cleanup garantizado */
    struct sigaction sa = {0};
    sa.sa_handler = on_signal; sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,&sa,NULL); sigaction(SIGTERM,&sa,NULL); sigaction(SIGQUIT,&sa,NULL);
    atexit(cleanup_all);

    /* SHM control */
    int fd_ctrl_local = shm_open(SHM_CONTROL, O_CREAT|O_RDWR, 0666);
    if (fd_ctrl_local<0){ perror("shm_open control"); return 1; }
    fd_ctrl = fd_ctrl_local;
    if (ftruncate(fd_ctrl, sizeof(control_shm))<0){ perror("ftruncate control"); return 1; }
    ctrl = mmap(NULL, sizeof(control_shm), PROT_READ|PROT_WRITE, MAP_SHARED, fd_ctrl, 0);
    if (ctrl==MAP_FAILED){ perror("mmap control"); return 1; }
    ctrl->next_id = 1;
    ctrl->max_id  = TOTAL;

    /* SHM cola */
    int fd_cola_local = shm_open(SHM_COLA, O_CREAT|O_RDWR, 0666);
    if (fd_cola_local<0){ perror("shm_open cola"); return 1; }
    fd_cola = fd_cola_local;
    if (ftruncate(fd_cola, sizeof(cola_shm))<0){ perror("ftruncate cola"); return 1; }
    cola = mmap(NULL, sizeof(cola_shm), PROT_READ|PROT_WRITE, MAP_SHARED, fd_cola, 0);
    if (cola==MAP_FAILED){ perror("mmap cola"); return 1; }
    cola->head = 0; cola->tail = 0;

    /* Semáforos */
    mutex_id     = sem_open(SEM_MUTEX_ID,  O_CREAT, 0644, 1);
    mutex_buffer = sem_open(SEM_MUTEX_BUF, O_CREAT, 0644, 1);
    sem_empty    = sem_open(SEM_EMPTY,     O_CREAT, 0644, BUFFER_SIZE);
    sem_full     = sem_open(SEM_FULL,      O_CREAT, 0644, 0);
    if (mutex_id == SEM_FAILED || mutex_buffer == SEM_FAILED || sem_empty == SEM_FAILED || sem_full == SEM_FAILED){
        perror("sem_open"); return 1;
    }

    /* Fork de generadores */
    for(int i=0;i<N;i++) {
        pid_t pid = fork();
        if (pid<0){ perror("fork"); return 1; }
        if(pid==0) {
            /* ---- Hijo: generador i ---- */
            const int gen_id = i;       // número de generador (0..N-1)
            srand(getpid());

            for(;;){
                if (stop_requested) _exit(0);

                int start, end;

                /* Pedir bloque de 10 IDs */
                int rc = wait_with_timeout(mutex_id, 2000);
                if (rc==1){ fprintf(stderr,"[gen%d] timeout mutex_id\n", gen_id); _exit(2); }
                if (rc<0){ perror("sem_timedwait(mutex_id)"); _exit(2); }

                if (ctrl->next_id > ctrl->max_id){
                    sem_post(mutex_id);
                    break;
                }
                start = ctrl->next_id;
                end   = start + BLOQUE - 1;
                if (end > ctrl->max_id) end = ctrl->max_id;
                ctrl->next_id = end + 1;

                sem_post(mutex_id);

                /* Generar y encolar */
                for(int id=start; id<=end; id++) {
                    if (stop_requested) _exit(0);

                    registro reg;
                    reg.id  = id;
                    reg.gen = gen_id;        // NUEVO
                    reg.pid = getpid();      // NUEVO
                    generar_nombre(reg.name);

                    rc = wait_with_timeout(sem_empty, 2000);
                    if (rc==1){ fprintf(stderr,"[gen%d] timeout sem_empty\n", gen_id); _exit(2); }
                    if (rc<0){ perror("sem_timedwait(sem_empty)"); _exit(2); }

                    rc = wait_with_timeout(mutex_buffer, 2000);
                    if (rc==1){ fprintf(stderr,"[gen%d] timeout mutex_buffer\n", gen_id); _exit(2); }
                    if (rc<0){ perror("sem_timedwait(mutex_buffer)"); _exit(2); }

                    cola->buffer[cola->tail] = reg;
                    cola->tail = (cola->tail + 1) % BUFFER_SIZE;

                    sem_post(mutex_buffer);
                    sem_post(sem_full);

                    sleep_ms(delay_ms);
                }
            }
            _exit(0);
        }
    }

    /* Padre = coordinador: abrir CSV y consumir */
    FILE *csv = fopen(CSV, "w");
    if(!csv) { perror("CSV"); return 1; }
    fprintf(csv, "id,generador,pid,Nombre\n");    // ENCABEZADO actualizado

    int escritos = 0;
    while(escritos < TOTAL && !stop_requested) {
        int rc = wait_with_timeout(sem_full, 3000);
        if (rc==1){
            /* timeout de consumo: si hijos ya terminaron y no hay más, salir */
            int status; int vivos = 0;
            for(int i=0;i<N;i++){
                pid_t r = waitpid(-1, &status, WNOHANG);
                if (r==0) { vivos=1; break; }                /* alguno sigue vivo */
                if (r>0) continue;                           /* reaped uno */
                if (r<0 && errno==ECHILD){ vivos=0; break; } /* no quedan hijos */
            }
            if (!vivos) break;
            else continue;
        }
        if (rc<0){ perror("sem_timedwait(sem_full)"); break; }

        if (wait_with_timeout(mutex_buffer, 2000)!=0){ perror("mutex_buffer"); break; }

        registro reg = cola->buffer[cola->head];
        cola->head = (cola->head + 1) % BUFFER_SIZE;

        sem_post(mutex_buffer);
        sem_post(sem_empty);

        /* Escritura con columnas nuevas */
        fprintf(csv, "%d,%d,%d,%s\n", reg.id, reg.gen, (int)reg.pid, reg.name);
        escritos++;
    }

    /* Esperar generadores restantes (si los hay) */
    while (wait(NULL) > 0) { /* no-op */ }

    fclose(csv);

    printf("Generación de registros completada. CSV: %s (escritos=%d)\n", CSV, escritos);
    return 0;
}
