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

int procesar_linea_protocolo(int fd, const char *linea);

/* estado de transacción (lock exclusivo sobre el CSV) */
static int csv_fd = -1;
static volatile int tx_active = 0;
static pthread_mutex_t tx_mtx = PTHREAD_MUTEX_INITIALIZER;

static int intentar_iniciar_tx(void)
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
  tx_active = 1;
  pthread_mutex_unlock(&tx_mtx);
  return 0;
}
static void finalizar_tx(void)
{
  pthread_mutex_lock(&tx_mtx);
  struct flock lk = {.l_type = F_UNLCK, .l_whence = SEEK_SET, .l_start = 0, .l_len = 0};
  (void)fcntl(csv_fd, F_SETLK, &lk);
  tx_active = 0;
  pthread_mutex_unlock(&tx_mtx);
}

/* worker por cliente */
static void *iniciar_thread_cliente(void *arg)
{
  int cfd = (int)(intptr_t)arg;
  FILE *in = fdopen(dup(cfd), "r");
  dprintf(cfd, "Conectado. Comandos: PING | GET <id> | ADD ... [producto=..] | UPDATE ... | DELETE <id> | BEGIN | COMMIT | ROLLBACK | QUIT\n");
  char linea[1024];
  while (fgets(linea, sizeof(linea), in))
  {
    if (!strncasecmp(linea, "QUIT", 4))
    {
      dprintf(cfd, "BYE\n");
      break;
    }
    if (!strncasecmp(linea, "BEGIN", 5))
    {
      if (intentar_iniciar_tx() == 0)
        dprintf(cfd, "OK\n");
      else
        dprintf(cfd, "ERR TX_ACTIVE\n");
      continue;
    }
    if (!strncasecmp(linea, "COMMIT", 6))
    {
      finalizar_tx();
      dprintf(cfd, "OK\n");
      continue;
    }
    if (!strncasecmp(linea, "ROLLBACK", 8))
    {
      finalizar_tx();
      dprintf(cfd, "OK\n");
      continue;
    }

    /* Si hay transacción activa, nadie puede consultar/modificar */
    pthread_mutex_lock(&tx_mtx);
    int denegar = tx_active;
    pthread_mutex_unlock(&tx_mtx);
    if (denegar)
    {
      dprintf(cfd, "ERR TX_ACTIVE\n");
      continue;
    }

    procesar_linea_protocolo(cfd, linea);
  }
  fclose(in);
  close(cfd);
  return NULL;
}

int main(int argc, char **argv)
{
  const char *host = "127.0.0.1";
  int port = 5000;
  int N = 4, M = 16;
  const char *csv = "../e1/productos.csv";
  for (int i = 1; i < argc; i++)
  {
    if (!strcmp(argv[i], "-H") && i + 1 < argc)
      host = argv[++i];
    else if (!strcmp(argv[i], "-p") && i + 1 < argc)
      port = atoi(argv[++i]);
    else if (!strcmp(argv[i], "-n") && i + 1 < argc)
      N = atoi(argv[++i]);
    else if (!strcmp(argv[i], "-m") && i + 1 < argc)
      M = atoi(argv[++i]);
    else if (!strcmp(argv[i], "-f") && i + 1 < argc)
      csv = argv[++i];
  }
  if (abrir_arch(csv) < 0)
  {
    fprintf(stderr, "ERR: no pude abrir CSV %s\n", csv);
    return 1;
  }
  csv_fd = open(csv, O_RDWR | O_CREAT, 0666);
  if (csv_fd < 0)
  {
    perror("open csv");
    return 1;
  }

  int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  int opt = 1;
  setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  struct sockaddr_in sa = {0};
  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);
  sa.sin_addr.s_addr = inet_addr(host);
  if (bind(socket_fd, (struct sockaddr *)&sa, sizeof(sa)) < 0)
  {
    perror("bind");
    return 1;
  }
  if (listen(socket_fd, M) < 0)
  {
    perror("listen");
    return 1;
  }

  printf("Servidor escuchando en %s:%d (N=%d, backlog=%d) CSV=%s\n", host, port, N, M, csv);
  pthread_t th;
  while (1)
  {
    int cfd = accept(socket_fd, NULL, NULL);
    if (cfd < 0)
    {
      if (errno == EINTR)
        continue;
      perror("accept");
      break;
    }
    pthread_create(&th, NULL, iniciar_thread_cliente, (void *)(intptr_t)cfd);
    pthread_detach(th);
  }
  close(socket_fd);
  cerrar_arch();
  close(csv_fd);
  return 0;
}
