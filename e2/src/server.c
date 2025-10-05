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

/* ==== PROTOS ==== */
int  procesar_linea_protocolo(int fd, const char *linea);
int  abrir_arch(const char *csv_path);
void cerrar_arch(void);
int  generar_snapshot(void);
int  guardar_snapshot(void);
int  descartar_snapshot(void);

/* ==== ESTADO GLOBAL DE TRANSACCIÓN ==== */
int csv_fd = -1;                     /* FD del CSV para fcntl locks */
int tx_active = 0;                   /* ¿hay transacción activa?     */
int tx_owner  = -1;                  /* socket (cfd) dueño de la TX  */
pthread_mutex_t tx_mtx = PTHREAD_MUTEX_INITIALIZER;

/* ==== CONTROL DE SERVIDOR / SEÑALES ==== */
static volatile sig_atomic_t g_stop = 0;
static int g_listen_fd = -1;

static void on_signal(int sig) {
  (void)sig;
  g_stop = 1;
  if (g_listen_fd >= 0) {
    /* Cerrar el socket de escucha rompe accept() y nos deja salir ordenado */
    close(g_listen_fd);
    g_listen_fd = -1;
  }
}

/* ==== TX HELPERS ==== */
static int intentar_iniciar_tx(int cfd) {
  pthread_mutex_lock(&tx_mtx);
  if (tx_active) {
    pthread_mutex_unlock(&tx_mtx);
    return -1;
  }

  struct flock lk = {
    .l_type   = F_WRLCK,
    .l_whence = SEEK_SET,
    .l_start  = 0,
    .l_len    = 0
  };

  if (fcntl(csv_fd, F_SETLK, &lk) < 0) {
    pthread_mutex_unlock(&tx_mtx);
    return -1;
  }

  /* Tras tomar el lock de archivo, crear snapshot in-memory */
  if (generar_snapshot() != 0) {
    struct flock lk2;
    memset(&lk2, 0, sizeof(lk2));
    lk2.l_type   = F_UNLCK;
    lk2.l_whence = SEEK_SET;
    lk2.l_start  = 0;
    lk2.l_len    = 0;
    (void)fcntl(csv_fd, F_SETLK, &lk2);
    pthread_mutex_unlock(&tx_mtx);
    return -1;
  }

  tx_active = 1;
  tx_owner  = cfd;
  pthread_mutex_unlock(&tx_mtx);
  return 0;
}

/* Cierra la TX (unlock archivo + limpiar flags) SOLO si cfd es el dueño */
static int finalizar_tx_owner(int cfd) {
  pthread_mutex_lock(&tx_mtx);
  if (!tx_active || tx_owner != cfd) {
    pthread_mutex_unlock(&tx_mtx);
    return -1; /* NOT_OWNER o NO_TX */
  }
  struct flock lk = {
    .l_type   = F_UNLCK,
    .l_whence = SEEK_SET,
    .l_start  = 0,
    .l_len    = 0
  };
  (void)fcntl(csv_fd, F_SETLK, &lk);
  tx_active = 0;
  tx_owner  = -1;
  pthread_mutex_unlock(&tx_mtx);
  return 0;
}

/* Rollback + unlock si el cfd es el dueño (se usa en desconexión y ROLLBACK) */
static void rollback_if_owner_and_unlock(int cfd) {
  pthread_mutex_lock(&tx_mtx);
  if (tx_active && tx_owner == cfd) {
    pthread_mutex_unlock(&tx_mtx); /* liberar para no anidar mientras llamamos a csvdb */
    (void)descartar_snapshot();    /* revertir snapshot (ignorar error) */
    pthread_mutex_lock(&tx_mtx);
    struct flock fl;
    memset(&fl, 0, sizeof(fl));
    fl.l_type   = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start  = 0;
    fl.l_len    = 0;
    (void)fcntl(csv_fd, F_SETLK, &fl);
    tx_active = 0;
    tx_owner  = -1;
  }
  pthread_mutex_unlock(&tx_mtx);
}

/* ==== WORKER POR CLIENTE ==== */
static void *iniciar_thread_cliente(void *arg) {
  int cfd = (int)(intptr_t)arg;
  FILE *arch = fdopen(dup(cfd), "r");
  if (!arch) {
    close(cfd);
    return NULL;
  }

  dprintf(cfd,
    "Conectado. Comandos: PING | GET <id> | FIND <nombre> | FIND ALL <nombre> | "
    "ADD ... | UPDATE ... | DELETE <id> | BEGIN | COMMIT | ROLLBACK | QUIT\n");

  char linea[1024];
  int quit = 0;

  while (!quit && fgets(linea, sizeof(linea), arch)) {
    /* Normalizar fin de línea */
    size_t L = strlen(linea);
    if (L && (linea[L-1] == '\n' || linea[L-1] == '\r')) {
      linea[L-1] = '\0';
    }

    /* Enfoque sin break/continue: cadena de if/else y flags */
    if (!strncasecmp(linea, "QUIT", 4)) {
      dprintf(cfd, "BYE\n");
      quit = 1;

    } else if (!strncasecmp(linea, "PING", 4)) {
      dprintf(cfd, "OK\n");

    } else if (!strncasecmp(linea, "BEGIN", 5)) {
      if (intentar_iniciar_tx(cfd) == 0) {
        dprintf(cfd, "OK\n");
      } else {
        dprintf(cfd, "ERR TX_ACTIVE\n");
      }

    } else if (!strncasecmp(linea, "COMMIT", 6)) {
      int es_duenio;
      pthread_mutex_lock(&tx_mtx);
      es_duenio = (tx_active && tx_owner == cfd);
      pthread_mutex_unlock(&tx_mtx);

      if (es_duenio) {
        int rc = guardar_snapshot();  /* persistir cambios */
        if (rc == 0) {
          if (finalizar_tx_owner(cfd) == 0) {
            dprintf(cfd, "OK\n");
          } else {
            dprintf(cfd, "ERR NOT_OWNER_OR_NO_TX\n");
          }
        } else {
          /* si falla persistencia, revertimos */
          rollback_if_owner_and_unlock(cfd);
          dprintf(cfd, "ERR COMMIT_FAILED\n");
        }
      } else {
        dprintf(cfd, "ERR NOT_OWNER_OR_NO_TX\n");
      }

    } else if (!strncasecmp(linea, "ROLLBACK", 8)) {
      int es_duenio;
      pthread_mutex_lock(&tx_mtx);
      es_duenio = (tx_active && tx_owner == cfd);
      pthread_mutex_unlock(&tx_mtx);

      if (es_duenio) {
        int rc = descartar_snapshot();
        rollback_if_owner_and_unlock(cfd);
        if (rc == 0) {
          dprintf(cfd, "OK\n");
        } else {
          dprintf(cfd, "ERR ROLLBACK_FAILED\n");
        }
      } else {
        dprintf(cfd, "ERR NOT_OWNER_OR_NO_TX\n");
      }

    } else {
      /* Si hay transacción activa y NO soy el dueño => denegar */
      int denegar;
      pthread_mutex_lock(&tx_mtx);
      denegar = (tx_active && tx_owner != cfd);
      pthread_mutex_unlock(&tx_mtx);

      if (!denegar) {
        procesar_linea_protocolo(cfd, linea);
      } else {
        dprintf(cfd, "ERR TX_ACTIVE\n");
      }
    }
  }

  /* Si salimos por EOF/desconexión y éramos dueños de TX → rollback + unlock */
  rollback_if_owner_and_unlock(cfd);

  if (arch) fclose(arch);
  close(cfd);
  return NULL;
}

/* ==== USO / PARSING ==== */
static void print_usage(const char *prog) {
  fprintf(stderr,
    "Uso: %s -H <host> -p <puerto> -n <hilos> -m <backlog> -f <archivo.csv>\n"
    "Ejemplo: %s -H 127.0.0.1 -p 5000 -n 4 -m 16 -f ./e1/productos.csv\n",
    prog, prog);
}

/* ==== MAIN ==== */
int main(int argc, char **argv) {
  const char *host = NULL;
  const char *csv_path = NULL;
  int port = 0, max_workers = 0, backlog = 0;

  /* Señales: salir ordenado y evitar abort por writes a sockets cerrados */
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = on_signal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT,  &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  signal(SIGPIPE, SIG_IGN);

  /* Parsing simple con getopt */
  int opt;
  while ((opt = getopt(argc, argv, "H:p:n:m:f:h")) != -1) {
    switch (opt) {
      case 'H': host        = optarg; break;
      case 'p': port        = atoi(optarg); break;
      case 'n': max_workers = atoi(optarg); break;
      case 'm': backlog     = atoi(optarg); break;
      case 'f': csv_path    = optarg; break;
      case 'h': print_usage(argv[0]); return 0;
      default:  print_usage(argv[0]); return 2;
    }
  }

  if (!host || port <= 0 || max_workers <= 0 || backlog <= 0 || !csv_path) {
    fprintf(stderr, "Error: parámetros inválidos o faltantes.\n");
    print_usage(argv[0]);
    return 2;
  }

  /* Validar que el CSV existe/abre para lectura al menos */
  FILE *csv_chk = fopen(csv_path, "r");
  if (!csv_chk) {
    fprintf(stderr, "Error: no puedo abrir CSV '%s': %s\n", csv_path, strerror(errno));
    return 2;
  }
  fclose(csv_chk);

  if (abrir_arch(csv_path) < 0) {
    fprintf(stderr, "ERR: abrir_arch('%s') falló\n", csv_path);
    return 1;
  }
  csv_fd = open(csv_path, O_RDWR | O_CREAT, 0666);
  if (csv_fd < 0) {
    perror("open csv");
    cerrar_arch();
    return 1;
  }

  g_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (g_listen_fd < 0) {
    perror("socket");
    close(csv_fd);
    cerrar_arch();
    return 1;
  }

  int optval = 1;
  (void)setsockopt(g_listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

  struct sockaddr_in sa_listen;
  memset(&sa_listen, 0, sizeof(sa_listen));
  sa_listen.sin_family = AF_INET;
  sa_listen.sin_port   = htons((uint16_t)port);
  sa_listen.sin_addr.s_addr = inet_addr(host);

  if (bind(g_listen_fd, (struct sockaddr *)&sa_listen, sizeof(sa_listen)) < 0) {
    perror("bind");
    close(g_listen_fd);
    g_listen_fd = -1;
    close(csv_fd);
    cerrar_arch();
    return 1;
  }

  if (listen(g_listen_fd, backlog) < 0) {
    perror("listen");
    close(g_listen_fd);
    g_listen_fd = -1;
    close(csv_fd);
    cerrar_arch();
    return 1;
  }

  printf("Servidor escuchando en %s:%d (threads=%d, backlog=%d) CSV=%s\n",
         host, port, max_workers, backlog, csv_path);

  /* Loop principal de aceptación sin break/continue */
  int running = 1;
  while (!g_stop && running) {
    int cfd = accept(g_listen_fd, NULL, NULL);
    if (cfd >= 0) {
      pthread_t th;
      (void)pthread_create(&th, NULL, iniciar_thread_cliente, (void *)(intptr_t)cfd);
      (void)pthread_detach(th);
    } else {
      /* error en accept */
      if (g_stop) {
        running = 0;         /* señal recibida → salir del loop */
      } else if (errno == EINTR) {
        /* nada: intentar nuevamente en la próxima iteración */
      } else {
        perror("accept");
        /* mantener el servidor vivo a menos que se decida lo contrario */
      }
    }
  }

  /* Salida ordenada del servidor */
  if (g_listen_fd >= 0) {
    close(g_listen_fd);
    g_listen_fd = -1;
  }

  /* Si queda una TX activa (raro), hacer rollback y unlock */
  rollback_if_owner_and_unlock(tx_owner);

  cerrar_arch();
  if (csv_fd >= 0) {
    close(csv_fd);
    csv_fd = -1;
  }
  return 0;
}
