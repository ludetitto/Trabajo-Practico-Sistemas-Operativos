#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <string.h>
#include <sys/wait.h>

#define BLOQUE 10
#define BUFFER_SIZE 10
#define MAX_NAME 32

typedef struct {
    int id;
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

// Función para generar nombre aleatorio
void generar_nombre(char *name) {
    const char *nombres[] = {"Ana","Juan","Luis","Marta","Pedro","Sofia","Diego","Clara"};
    int idx = rand() % 8;
    strncpy(name, nombres[idx], MAX_NAME);
}

int main(int argc, char *argv[]) {
    if(argc != 3) {
        printf("Uso: %s <cantidad_generadores> <total_registros>\n", argv[0]);
        return 1;
    }

    int N = atoi(argv[1]);     
    int TOTAL = atoi(argv[2]); 
    if(N <=0 || TOTAL <=0) {
        printf("Parámetros inválidos.\n");
        return 1;
    }

    // --- Memoria compartida ---
    const char *shm_control = "/shm_control";
    const char *shm_cola = "/shm_cola";

    int fd_ctrl = shm_open(shm_control, O_CREAT | O_RDWR, 0666);
    ftruncate(fd_ctrl, sizeof(control_shm));
    control_shm *ctrl = mmap(NULL, sizeof(control_shm), PROT_READ | PROT_WRITE, MAP_SHARED, fd_ctrl, 0);
    ctrl->next_id = 1;
    ctrl->max_id = TOTAL;

    int fd_cola = shm_open(shm_cola, O_CREAT | O_RDWR, 0666);
    ftruncate(fd_cola, sizeof(cola_shm));
    cola_shm *cola = mmap(NULL, sizeof(cola_shm), PROT_READ | PROT_WRITE, MAP_SHARED, fd_cola, 0);
    cola->head = 0;
    cola->tail = 0;

    // --- Semáforos ---
    sem_t *mutex_id = sem_open("/mutex_id", O_CREAT, 0644, 1);
    sem_t *mutex_buffer = sem_open("/mutex_buffer", O_CREAT, 0644, 1);
    sem_t *sem_empty = sem_open("/sem_empty", O_CREAT, 0644, BUFFER_SIZE);
    sem_t *sem_full  = sem_open("/sem_full", O_CREAT, 0644, 0);

    // --- Crear generadores ---
    for(int i=0;i<N;i++) {
        pid_t pid = fork();
        if(pid==0) {
            srand(getpid()); // semilla para nombres aleatorios
            while(1) {
                int start, end;

                // Pedir bloque de 10 IDs
                sem_wait(mutex_id);
                if(ctrl->next_id > ctrl->max_id) {
                    sem_post(mutex_id);
                    break;
                }
                start = ctrl->next_id;
                end = start + BLOQUE -1;
                if(end > ctrl->max_id) end = ctrl->max_id;
                ctrl->next_id = end + 1;
                sem_post(mutex_id);

                // Generar registros y poner en cola
                for(int id=start; id<=end; id++) {
                    registro reg;
                    reg.id = id;
                    generar_nombre(reg.name);

                    sem_wait(sem_empty);
                    sem_wait(mutex_buffer);

                    cola->buffer[cola->tail] = reg;
                    cola->tail = (cola->tail + 1) % BUFFER_SIZE;

                    sem_post(mutex_buffer);
                    sem_post(sem_full);

                    usleep(500000); // 0.5 segundos para monitoreo
                }
            }
            exit(0);
        }
    }

    // --- SOLO EL PADRE: abrir CSV y escribir ---
    FILE *csv = fopen("registros.csv", "w");
    if(!csv) { perror("CSV"); return 1; }
    fprintf(csv, "ID,Nombre\n");   // <- Encabezado UNA VEZ

    // --- Coordinador: consumir registros ---
    int registros_escritos = 0;
    while(registros_escritos < TOTAL) {
        sem_wait(sem_full);
        sem_wait(mutex_buffer);

        registro reg = cola->buffer[cola->head];
        cola->head = (cola->head + 1) % BUFFER_SIZE;

        sem_post(mutex_buffer);
        sem_post(sem_empty);

        fprintf(csv, "%d,%s\n", reg.id, reg.name);
        registros_escritos++;
    }

    // --- Esperar generadores ---
    for(int i=0;i<N;i++) wait(NULL);

    fclose(csv);

    // --- Limpiar ---
    sem_close(mutex_id); sem_unlink("/mutex_id");
    sem_close(mutex_buffer); sem_unlink("/mutex_buffer");
    sem_close(sem_empty); sem_unlink("/sem_empty");
    sem_close(sem_full);  sem_unlink("/sem_full");

    munmap(ctrl,sizeof(control_shm)); shm_unlink(shm_control);
    munmap(cola,sizeof(cola_shm)); shm_unlink(shm_cola);

    printf("Generación de registros completada. CSV listo.\n");

    return 0;
}
