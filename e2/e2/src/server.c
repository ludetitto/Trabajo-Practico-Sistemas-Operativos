#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <strings.h> // strncasecmp
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../include/csvdb.h"
#include <sys/stat.h>
#include <fcntl.h>

/* ==== PROTOS ==== */
int procesar_linea_protocolo(int fd, const char *linea);
int abrir_arch(const char *csv_path);
void cerrar_arch(void);
int generar_snapshot(void);
int guardar_snapshot(void);
int descartar_snapshot(void);

/* ==== ESTADO GLOBAL DE TRANSACCIÓN ==== */
int csv_fd = -1;
int tx_active = 0;
int tx_owner = -1;
pthread_mutex_t tx_mtx = PTHREAD_MUTEX_INITIALIZER;

/* ==== CONTROL DE SERVIDOR ==== */
static volatile sig_atomic_t g_stop = 0;
static int g_listen_fd = -1;

/* ==== CONTROL DE CLIENTES SIMULTÁNEOS ==== */
static int active_clients = 0;
static int max_clients = 0;
static pthread_mutex_t mx_clients = PTHREAD_MUTEX_INITIALIZER;

/* Cola dinámica de espera (tamaño = backlog pasado por -m) */
static int *cola_espera = NULL;
static int capacidad_espera = 0;
static int frente_espera = 0, en_espera = 0; // antes, estaba "fin_espera = 0" pero lo quite porque estaba sin usar
static pthread_cond_t cond_espera = PTHREAD_COND_INITIALIZER;

/* ==== MANEJO DE SEÑALES ==== */
static void on_signal(int sig)
{
  (void)sig;
  g_stop = 1;
  if (g_listen_fd >= 0)
  {
    close(g_listen_fd);
    g_listen_fd = -1;
  }
}

/* ==== TX HELPERS ==== */
static int intentar_iniciar_tx(int cfd)
{
  pthread_mutex_lock(&tx_mtx);
  if (tx_active)
  {
    pthread_mutex_unlock(&tx_mtx);
    return -1;
  }

  struct flock lk = {.l_type = F_WRLCK, .l_whence = SEEK_SET, .l_start = 0, .l_len = 0};

  if (fcntl(csv_fd, F_SETLK, &lk) < 0)
  {
    pthread_mutex_unlock(&tx_mtx);
    return -1;
  }

  if (generar_snapshot() != 0)
  {
    struct flock lk2 = {.l_type = F_UNLCK, .l_whence = SEEK_SET, .l_start = 0, .l_len = 0};
    (void)fcntl(csv_fd, F_SETLK, &lk2);
    pthread_mutex_unlock(&tx_mtx);
    return -1;
  }

  tx_active = 1;
  tx_owner = cfd;
  pthread_mutex_unlock(&tx_mtx);
  return 0;
}

static int finalizar_tx_owner(int cfd)
{
  pthread_mutex_lock(&tx_mtx);
  if (!tx_active || tx_owner != cfd)
  {
    pthread_mutex_unlock(&tx_mtx);
    return -1;
  }
  struct flock lk = {.l_type = F_UNLCK, .l_whence = SEEK_SET, .l_start = 0, .l_len = 0};
  (void)fcntl(csv_fd, F_SETLK, &lk);
  tx_active = 0;
  tx_owner = -1;
  pthread_mutex_unlock(&tx_mtx);
  return 0;
}

static void rollback_valido(int cfd)
{
  pthread_mutex_lock(&tx_mtx);
  if (tx_active && tx_owner == cfd)
  {
    pthread_mutex_unlock(&tx_mtx);
    (void)descartar_snapshot();
    pthread_mutex_lock(&tx_mtx);
    struct flock fl = {.l_type = F_UNLCK, .l_whence = SEEK_SET, .l_start = 0, .l_len = 0};
    (void)fcntl(csv_fd, F_SETLK, &fl);
    tx_active = 0;
    tx_owner = -1;
  }
  pthread_mutex_unlock(&tx_mtx);
}

/* ==== WORKER POR CLIENTE ==== */
static void *iniciar_thread_cliente(void *arg)
{
  int cfd = (int)(intptr_t)arg;

  /* === Admisión y control de espera === */
  pthread_mutex_lock(&mx_clients);

  // si el cupo está lleno y hay lugar en cola
  if (active_clients >= max_clients)
  {
    if (en_espera >= capacidad_espera)
    {
      pthread_mutex_unlock(&mx_clients);
      dprintf(cfd, "ERR BUSY\n");
      close(cfd);
      return NULL;
    }

    en_espera++;
    int pos = en_espera;
    dprintf(cfd, "En espera de disponibilidad del servidor... (%d/%d)\n",
            pos, capacidad_espera);
    fprintf(stdout, "[SRV] Cliente agregado a espera (%d/%d)\n", en_espera, capacidad_espera);
    fflush(stdout);

    while (active_clients >= max_clients || frente_espera != 0)
      pthread_cond_wait(&cond_espera, &mx_clients);

    en_espera--;
  }

  active_clients++;
  fprintf(stdout, "[SRV] Cliente aceptado. Activos=%d, Espera=%d/%d\n",
          active_clients, en_espera, capacidad_espera);
  pthread_mutex_unlock(&mx_clients);

  /* === Lógica normal === */
  FILE *arch = fdopen(dup(cfd), "r");
  if (!arch)
  {
    close(cfd);
    pthread_mutex_lock(&mx_clients);
    active_clients--;
    pthread_cond_broadcast(&cond_espera);
    pthread_mutex_unlock(&mx_clients);
    return NULL;
  }

  dprintf(cfd,
          "╔════════════════════════════════════════════════════════════╗\n"
          "║     Comandos disponibles                                   ║\n"
          "╠════════════════════════════════════════════════════════════╣\n"
          "║  PING                                                      ║\n"
          "║  GET <id>                                                  ║\n"
          "║  FIND <nombre>                                             ║\n"
          "║  FIND ALL <nombre>                                         ║\n"
          "║  ADD nombre= <nombre> precio= <precio> stock= <stock>      ║ \n"
          "║  UPDATE nombre= <nombre> precio= <precio> stock= <stock>   ║\n"
          "║  DELETE <id>                                               ║\n"
          "║  BEGIN TRANSACTION                                         ║\n"
          "║  COMMIT TRANSACTION                                        ║\n"
          "║  ROLLBACK TRANSACTION                                      ║\n"
          "║  QUIT                                                      ║\n"
          "╚════════════════════════════════════════════════════════════╝\n");

  char linea[1024];
  int quit = 0;

  while (!quit && fgets(linea, sizeof(linea), arch))
  {
    size_t L = strlen(linea);
    if (L && (linea[L - 1] == '\n' || linea[L - 1] == '\r'))
      linea[L - 1] = '\0';

    if (!strncasecmp(linea, "QUIT", 4))
    {
      dprintf(cfd, "BYE\n");
      quit = 1;
    }
    else if (!strncasecmp(linea, "PING", 4))
    {
      dprintf(cfd, "OK\n");
    }
    else if (!strncasecmp(linea, "BEGIN TRANSACTION", 17))
    {
      dprintf(cfd, intentar_iniciar_tx(cfd) == 0 ? "OK\n" : "ERR TX_ACTIVE\n");
    }
    else if (!strncasecmp(linea, "COMMIT TRANSACTION", 18))
    {
      int es_duenio;
      pthread_mutex_lock(&tx_mtx);
      es_duenio = (tx_active && tx_owner == cfd);
      pthread_mutex_unlock(&tx_mtx);

      if (es_duenio)
      {
        int rc = guardar_snapshot();
        if (rc == 0)
        {
          if (finalizar_tx_owner(cfd) == 0)
            dprintf(cfd, "OK\n");
          else
            dprintf(cfd, "ERR NOT_OWNER_OR_NO_TX\n");
        }
        else
        {
          rollback_valido(cfd);
          dprintf(cfd, "ERR COMMIT_FAILED\n");
        }
      }
      else
        dprintf(cfd, "ERR NOT_OWNER_OR_NO_TX\n");
    }
    else if (!strncasecmp(linea, "HELP", 4))
    {
      dprintf(cfd,
              "╔════════════════════════════════════════════════════╗\n"
              "║                Comandos disponibles:               ║\n"
              "╠════════════════════════════════════════════════════╣\n"
              "║  PING                        - Test de conexión    ║\n"
              "║  GET <id>                    - Buscar por ID       ║\n"
              "║  FIND <nombre>               - Buscar por nombre   ║\n"
              "║  FIND ALL <nombre>           - Buscar todos        ║\n"
              "║  ADD nombre=... precio=...   - Agregar registro    ║\n"
              "║  UPDATE ...                  - Modificar registro  ║\n"
              "║  DELETE <id>                 - Eliminar registro   ║\n"
              "║  BEGIN TRANSACTION           - Iniciar transacción ║\n"
              "║  COMMIT TRANSACTION          - Confirmar cambios   ║\n"
              "║  ROLLBACK TRANSACTION        - Deshacer cambios    ║\n"
              "║  QUIT                        - Salir               ║\n"
              "╚════════════════════════════════════════════════════╝\n");
    }
    else if (!strncasecmp(linea, "ROLLBACK TRANSACTION", 20))
    {
      int es_duenio;
      pthread_mutex_lock(&tx_mtx);
      es_duenio = (tx_active && tx_owner == cfd);
      pthread_mutex_unlock(&tx_mtx);

      if (es_duenio)
      {
        int rc = descartar_snapshot();
        rollback_valido(cfd);
        dprintf(cfd, rc == 0 ? "OK\n" : "ERR ROLLBACK_FAILED\n");
      }
      else
        dprintf(cfd, "ERR NOT_OWNER_OR_NO_TX\n");
    }
    else
    {
      int es_find = !strncasecmp(linea, "FIND", 4);
      int es_get = !strncasecmp(linea, "GET", 3);
      int denegar;
      pthread_mutex_lock(&tx_mtx);
      denegar = (tx_active && tx_owner != cfd && !es_find && !es_get);
      pthread_mutex_unlock(&tx_mtx);
      if (!denegar)
        procesar_linea_protocolo(cfd, linea);
      else
        dprintf(cfd, "ERR TX_ACTIVE\n");
    }
  }

  rollback_valido(cfd);
  if (arch)
    fclose(arch);
  close(cfd);

  pthread_mutex_lock(&mx_clients);
  active_clients--;
  pthread_cond_signal(&cond_espera); /* libera un slot para quien espera */
  fprintf(stdout, "[SRV] Cliente desconectado. Activos ahora: %d\n", active_clients);
  pthread_mutex_unlock(&mx_clients);
  return NULL;
}

/* ==== USO ==== */
static void print_usage(const char *prog)
{
  fprintf(stderr,
          "Uso: %s -H <host> -p <puerto> -n <clientes> -m <backlog> -f <archivo.csv>\n"
          "Ejemplo: %s -H 127.0.0.1 -p 5000 -n 4 -m 16 -f ../productos.csv\n",
          prog, prog);
}

/* ==== MAIN ==== */
int main(int argc, char **argv)
{
  const char *host = NULL, *csv_path = NULL;
  int port = 0, backlog = 0;

  struct sigaction sa = {0};
  sa.sa_handler = on_signal;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  signal(SIGPIPE, SIG_IGN);

  int opt;
  while ((opt = getopt(argc, argv, "H:p:n:m:f:h")) != -1)
  {
    switch (opt)
    {
    case 'H':
      host = optarg;
      break;
    case 'p':
      port = atoi(optarg);
      break;
    case 'n':
      max_clients = atoi(optarg);
      break;
    case 'm':
      backlog = atoi(optarg);
      break;
    case 'f':
      csv_path = optarg;
      break;
    case 'h':
      print_usage(argv[0]);
      return 0;
    default:
      print_usage(argv[0]);
      return 2;
    }
  }

  if (!host || port <= 0 || max_clients <= 0 || backlog <= 0 || !csv_path)
  {
    fprintf(stderr, "Error: parámetros inválidos o faltantes.\n");
    print_usage(argv[0]);
    return 2;
  }

  /* Crear cola dinámica según backlog */
  capacidad_espera = backlog;
  cola_espera = calloc((size_t)capacidad_espera, sizeof(int));
  if (!cola_espera)
  {
    fprintf(stderr, "Error: no se pudo reservar memoria para cola de espera (%d)\n",
            capacidad_espera);
    return 1;
  }

  if (abrir_arch(csv_path) < 0)
  {
    fprintf(stderr, "ERR: abrir_arch('%s') falló\n", csv_path);
    return 1;
  }
  csv_fd = open(csv_path, O_RDWR | O_CREAT, 0666);
  if (csv_fd < 0)
  {
    perror("open csv");
    cerrar_arch();
    return 1;
  }

  g_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (g_listen_fd < 0)
  {
    perror("socket");
    close(csv_fd);
    cerrar_arch();
    return 1;
  }

  int optval = 1;
  setsockopt(g_listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

  struct sockaddr_in sa_listen = {0};
  sa_listen.sin_family = AF_INET;
  sa_listen.sin_port = htons((uint16_t)port);
  sa_listen.sin_addr.s_addr = inet_addr(host);

  if (bind(g_listen_fd, (struct sockaddr *)&sa_listen, sizeof(sa_listen)) < 0)
  {
    perror("bind");
    close(g_listen_fd);
    close(csv_fd);
    cerrar_arch();
    return 1;
  }

  if (listen(g_listen_fd, backlog) < 0)
  {
    perror("listen");
    close(g_listen_fd);
    exit(EXIT_FAILURE);
  }

  fprintf(stdout, "[SRV] Escuchando en %s:%d (max activos=%d, backlog=%d)\n",
          host, port, max_clients, backlog);
  fflush(stdout);

  while (!g_stop)
  {
    int cfd = accept(g_listen_fd, NULL, NULL);
    if (cfd < 0)
    {
      if (errno == EINTR)
        continue;
      if (g_stop)
        break;
      perror("accept");
      continue;
    }

    pthread_t th;
    pthread_create(&th, NULL, iniciar_thread_cliente, (void *)(intptr_t)cfd);
    pthread_detach(th);
  }

  if (g_listen_fd >= 0)
  {
    close(g_listen_fd);
    g_listen_fd = -1;
  }

  rollback_valido(tx_owner);
  cerrar_arch();
  if (csv_fd >= 0)
    close(csv_fd);

  if (cola_espera)
  {
    free(cola_espera);
    cola_espera = NULL;
  }
  fprintf(stdout, "[SRV] Servidor detenido.\n");
  return 0;
}

/* ==== FIN DE server.c ==== */