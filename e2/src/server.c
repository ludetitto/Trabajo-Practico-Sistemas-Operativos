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

int proto_handle_line(int fd, const char *line);

/* estado de transacción (lock exclusivo sobre el CSV) */
static int g_fd_csv = -1;
static volatile int g_tx_active = 0;
static pthread_mutex_t g_tx_mtx = PTHREAD_MUTEX_INITIALIZER;

static int try_begin_tx(void)
{
  pthread_mutex_lock(&g_tx_mtx);
  if (g_tx_active)
  {
    pthread_mutex_unlock(&g_tx_mtx);
    return -1;
  }
  struct flock lk = {.l_type = F_WRLCK, .l_whence = SEEK_SET, .l_start = 0, .l_len = 0};
  if (fcntl(g_fd_csv, F_SETLK, &lk) < 0)
  {
    pthread_mutex_unlock(&g_tx_mtx);
    return -1;
  }
  g_tx_active = 1;
  pthread_mutex_unlock(&g_tx_mtx);
  return 0;
}
static void end_tx(void)
{
  pthread_mutex_lock(&g_tx_mtx);
  struct flock lk = {.l_type = F_UNLCK, .l_whence = SEEK_SET, .l_start = 0, .l_len = 0};
  (void)fcntl(g_fd_csv, F_SETLK, &lk);
  g_tx_active = 0;
  pthread_mutex_unlock(&g_tx_mtx);
}

/* worker por cliente */
static void *client_thr(void *arg)
{
  int cfd = (int)(intptr_t)arg;
  FILE *in = fdopen(dup(cfd), "r");
  dprintf(cfd, "Conectado. Comandos: PING | GET <id> | ADD ... [evento=..] | UPDATE ... | DELETE id=.. | SHOWQ evento=.. | ATTEND evento=.. | LEAVE id=.. | BEGIN | COMMIT | ROLLBACK | QUIT\n");
  char line[1024];
  while (fgets(line, sizeof(line), in))
  {
    if (!strncasecmp(line, "QUIT", 4))
    {
      dprintf(cfd, "BYE\n");
      break;
    }
    if (!strncasecmp(line, "BEGIN", 5))
    {
      if (try_begin_tx() == 0)
        dprintf(cfd, "OK\n");
      else
        dprintf(cfd, "ERR TX_ACTIVE\n");
      continue;
    }
    if (!strncasecmp(line, "COMMIT", 6))
    {
      end_tx();
      dprintf(cfd, "OK\n");
      continue;
    }
    if (!strncasecmp(line, "ROLLBACK", 8))
    {
      end_tx();
      dprintf(cfd, "OK\n");
      continue;
    }

    /* Si hay transacción activa, nadie puede consultar/modificar */
    pthread_mutex_lock(&g_tx_mtx);
    int deny = g_tx_active;
    pthread_mutex_unlock(&g_tx_mtx);
    if (deny)
    {
      dprintf(cfd, "ERR TX_ACTIVE\n");
      continue;
    }

    proto_handle_line(cfd, line);
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
  const char *csv = "../e1/db.csv";
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
  if (db_open(csv) < 0)
  {
    fprintf(stderr, "ERR: no pude abrir CSV %s\n", csv);
    return 1;
  }
  g_fd_csv = open(csv, O_RDWR | O_CREAT, 0666);
  if (g_fd_csv < 0)
  {
    perror("open csv");
    return 1;
  }

  int sfd = socket(AF_INET, SOCK_STREAM, 0);
  int opt = 1;
  setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  struct sockaddr_in sa = {0};
  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);
  sa.sin_addr.s_addr = inet_addr(host);
  if (bind(sfd, (struct sockaddr *)&sa, sizeof(sa)) < 0)
  {
    perror("bind");
    return 1;
  }
  if (listen(sfd, M) < 0)
  {
    perror("listen");
    return 1;
  }

  printf("Servidor escuchando en %s:%d (N=%d, backlog=%d) CSV=%s\n", host, port, N, M, csv);
  pthread_t th;
  while (1)
  {
    int cfd = accept(sfd, NULL, NULL);
    if (cfd < 0)
    {
      if (errno == EINTR)
        continue;
      perror("accept");
      break;
    }
    pthread_create(&th, NULL, client_thr, (void *)(intptr_t)cfd);
    pthread_detach(th);
  }
  close(sfd);
  db_close();
  close(g_fd_csv);
  return 0;
}
