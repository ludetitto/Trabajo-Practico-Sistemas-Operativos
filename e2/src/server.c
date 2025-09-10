#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <strings.h>   // strncasecmp
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../include/csvdb.h"

int procesar_linea_protocolo(int fd, const char *linea);

/* ===== Estado de transacción (lock exclusivo del CSV) ===== */
int csv_fd = -1;                       // fd del CSV para fcntl()
int tx_active = 0;                     // hay TX abierta?
int tx_owner  = -1;                    // cfd del dueño de la TX
pthread_mutex_t tx_mtx = PTHREAD_MUTEX_INITIALIZER;  // protege estado TX

/* ===== Control de concurrencia (máx. N clientes atendidos) ===== */
static pthread_mutex_t conc_mtx = PTHREAD_MUTEX_INITIALIZER;
static int conc = 0;                  // clientes atendidos actualmente
static int Nmax = 0;                  // límite configurado con -n

/* ===== Iniciar TX: toma F_WRLCK + crea snapshot ===== */
static int intentar_iniciar_tx(int cfd) {
    pthread_mutex_lock(&tx_mtx);

    if (tx_active) {                  // ya hay una TX
        pthread_mutex_unlock(&tx_mtx);
        return -1;
    }

    struct flock lk; memset(&lk, 0, sizeof(lk));
    lk.l_type   = F_WRLCK;            // lock exclusivo
    lk.l_whence = SEEK_SET;
    lk.l_start  = 0;
    lk.l_len    = 0;                  // hasta EOF

    if (fcntl(csv_fd, F_SETLK, &lk) == -1) {   // intento de lock
        pthread_mutex_unlock(&tx_mtx);
        return -1;
    }

    tx_active = 1;
    tx_owner  = cfd;

    if (csvdb_begin_snapshot() != 0) {         // snapshot in-memory
        struct flock lk2; memset(&lk2, 0, sizeof(lk2));
        lk2.l_type = F_UNLCK; lk2.l_whence = SEEK_SET; lk2.l_start = 0; lk2.l_len = 0;
        (void)fcntl(csv_fd, F_SETLK, &lk2);
        tx_active = 0; tx_owner = -1;
        pthread_mutex_unlock(&tx_mtx);
        return -1;
    }

    pthread_mutex_unlock(&tx_mtx);
    return 0;
}

/* ===== Finalizar TX: libera F_UNLCK + limpia flags ===== */
static void finalizar_tx(void) {
    pthread_mutex_lock(&tx_mtx);

    struct flock lk; memset(&lk, 0, sizeof(lk));
    lk.l_type   = F_UNLCK;
    lk.l_whence = SEEK_SET;
    lk.l_start  = 0;
    lk.l_len    = 0;
    (void)fcntl(csv_fd, F_SETLK, &lk);

    tx_active = 0;
    tx_owner  = -1;

    pthread_mutex_unlock(&tx_mtx);
}

/* ===== Worker por cliente ===== */
static void *iniciar_thread_cliente(void *arg) {
    int cfd = (int)(intptr_t)arg, denegar;
    FILE *arch = fdopen(dup(cfd), "r");        // stream de lectura del socket
    dprintf(cfd, "Conectado. Comandos: PING | GET <id> | ADD ... | UPDATE ... | DELETE <id> | BEGIN | COMMIT | ROLLBACK | QUIT\n");
    char linea[1024];

    while (fgets(linea, sizeof(linea), arch)) {
        if (!strncasecmp(linea, "QUIT", 4)) {  // cierre amable
            dprintf(cfd, "BYE\n");
            break;
        }
        if (!strncasecmp(linea, "PING", 4)) {  // heartbeat
            dprintf(cfd, "OK\n");
            continue;
        }
        if (!strncasecmp(linea, "BEGIN", 5)) { // abre TX
            if (!intentar_iniciar_tx(cfd)) dprintf(cfd, "OK\n");
            else                            dprintf(cfd, "ERR TX_ACTIVE\n");
            continue;
        }
        if (!strncasecmp(linea, "COMMIT", 6)) { // confirma TX
            if (!tx_active)            dprintf(cfd, "ERR NOT_TX_ACTIVE\n");
            else if (tx_owner != cfd)  dprintf(cfd, "ERR NOT_OWNER\n");
            else {
                (void)csvdb_commit_snapshot(); // descarta snapshot (ya persististe en DML)
                finalizar_tx();                 // libera lock + flags
                dprintf(cfd, "OK\n");
            }
            continue;
        }
        if (!strncasecmp(linea, "ROLLBACK", 8)) { // revierte TX
            if (!tx_active)           { dprintf(cfd, "ERR NOT_TX_ACTIVE\n"); continue; }
            if (tx_owner != cfd)      { dprintf(cfd, "ERR NOT_OWNER\n");     continue; }
            int rc = csvdb_rollback_snapshot();   // restaura snapshot + guarda CSV
            finalizar_tx();
            dprintf(cfd, (rc == 0) ? "OK\n" : "ERR ROLLBACK_FAILED\n");
            continue;
        }

        /* Política de bloqueo: si hay TX y no soy dueño -> denegar */
        pthread_mutex_lock(&tx_mtx);
        denegar = (tx_active && tx_owner != cfd);
        pthread_mutex_unlock(&tx_mtx);

        if (!denegar) procesar_linea_protocolo(cfd, linea);  // procesa GET/ADD/UPDATE/DELETE
        else           dprintf(cfd, "ERR TX_ACTIVE\n");
    }

    /* Si el dueño se desconecta sin COMMIT -> ROLLBACK automático */
    pthread_mutex_lock(&tx_mtx);
    int era_duenio = (tx_active && tx_owner == cfd);
    pthread_mutex_unlock(&tx_mtx);
    if (era_duenio) {
        (void)csvdb_rollback_snapshot();
        finalizar_tx();
    }

    fclose(arch);
    close(cfd);

    /* Decrementa clientes concurrentes al salir */
    pthread_mutex_lock(&conc_mtx);
    if (conc > 0) conc--;
    pthread_mutex_unlock(&conc_mtx);

    return NULL;
}

/* ===== Main: parseo de flags, socket y accept-loop con cupo N ===== */
int main(int argc, char **argv) {
    const char *host = "127.0.0.1";
    int port = 5000;
    int N = 4, M = 16;                          // N concurrentes, M backlog
    const char *csv = "../e1/productos.csv";

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-H") && i + 1 < argc)      host = argv[++i];
        else if (!strcmp(argv[i], "-p") && i + 1 < argc) port = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-n") && i + 1 < argc) N = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-m") && i + 1 < argc) M = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-f") && i + 1 < argc) csv = argv[++i];
    }
    Nmax = (N > 0 ? N : 1);                      // aplica límite N

    if (abrir_arch(csv) < 0) {                   // carga CSV a memoria
        fprintf(stderr, "ERR: no pude abrir CSV %s\n", csv);
        return 1;
    }
    csv_fd = open(csv, O_RDWR | O_CREAT, 0666);  // fd para fcntl()
    if (csv_fd < 0) { perror("open csv"); return 1; }

    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1; setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in sa = {0};
    sa.sin_family = AF_INET;
    sa.sin_port   = htons(port);
    sa.sin_addr.s_addr = inet_addr(host);
    if (bind(socket_fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) { perror("bind");   return 1; }
    if (listen(socket_fd, M) < 0) { perror("listen"); return 1; }  // backlog = M

    printf("Servidor escuchando en %s:%d (N=%d, backlog=%d) CSV=%s\n", host, port, Nmax, M, csv);
    pthread_t th;

    while (1) {
        int cfd = accept(socket_fd, NULL, NULL);
        if (cfd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            break;
        }

        /* Control de cupo: si estamos en N, rechazar y cerrar */
        int rechazar = 0;
        pthread_mutex_lock(&conc_mtx);
        if (conc >= Nmax) rechazar = 1;
        else              conc++;
        pthread_mutex_unlock(&conc_mtx);

        if (rechazar) {
            dprintf(cfd, "ERR SERVER_BUSY\n");
            close(cfd);
            continue;
        }

        pthread_create(&th, NULL, iniciar_thread_cliente, (void *)(intptr_t)cfd);
        pthread_detach(th);
    }

    close(socket_fd);
    cerrar_arch();
    close(csv_fd);
    return 0;
}
