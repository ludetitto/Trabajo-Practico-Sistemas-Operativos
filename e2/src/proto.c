#define _GNU_SOURCE 1
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <pthread.h>
#include <unistd.h>
#include "../include/proto.h"
#include "../include/csvdb.h"

static void rstrip(char *s)
{
  size_t n = strlen(s);
  while (n && (s[n - 1] == '\n' || s[n - 1] == '\r' || isspace((unsigned char)s[n - 1])))
    s[--n] = 0;
}
static void lskip(const char **ps)
{
  const char *p = *ps;
  while (*p && isspace((unsigned char)*p))
    p++;
  *ps = p;
}
static void up(char *s)
{
  for (; *s; s++)
    *s = (char)toupper((unsigned char)*s);
}

static int kv_get(const char *line, const char *key, char *out, size_t outsz)
{
  char pat[64];
  snprintf(pat, sizeof(pat), "%s=", key);
  const char *p = strcasestr(line, pat);
  if (!p)
    return -1;
  p += strlen(pat);
  size_t i = 0;
  while (*p && !isspace((unsigned char)*p) && i + 1 < outsz)
    out[i++] = *p++;
  out[i] = 0;
  return (int)i;
}

/* ===== Transacciones (lock global) ===== */
static pthread_mutex_t tx_mtx = PTHREAD_MUTEX_INITIALIZER;
static int tx_activa = 0;
static int tx_owner_fd = -1;

static int tx_begin(int fd)
{
  pthread_mutex_lock(&tx_mtx);
  if (tx_activa && tx_owner_fd != fd)
  {
    pthread_mutex_unlock(&tx_mtx);
    return -1;
  }
  tx_activa = 1;
  tx_owner_fd = fd;
  pthread_mutex_unlock(&tx_mtx);
  return 0;
}
static int tx_owner_only(int fd)
{
  pthread_mutex_lock(&tx_mtx);
  int ok = (tx_activa && tx_owner_fd == fd);
  pthread_mutex_unlock(&tx_mtx);
  return ok ? 0 : -1;
}
static int tx_other_active(int fd)
{
  pthread_mutex_lock(&tx_mtx);
  int busy = (tx_activa && tx_owner_fd != fd);
  pthread_mutex_unlock(&tx_mtx);
  return busy ? 0 : -1;
}
static void tx_end(int fd)
{
  pthread_mutex_lock(&tx_mtx);
  if (tx_activa && tx_owner_fd == fd)
  {
    tx_activa = 0;
    tx_owner_fd = -1;
  }
  pthread_mutex_unlock(&tx_mtx);
}

/* ===== Handler de líneas ===== */
void proto_handle_line(int cfd, char *line)
{
  rstrip(line);
  const char *p = line;
  lskip(&p);
  if (!*p)
  {
    dprintf(cfd, "ERR EMPTY\n");
    return;
  }

  char cmd[32] = {0};
  int i = 0;
  while (p[i] && !isspace((unsigned char)p[i]) && i < (int)sizeof(cmd) - 1)
  {
    cmd[i] = p[i];
    i++;
  }
  cmd[i] = 0;
  up(cmd);
  p += i;
  lskip(&p);

  /* Lecturas: PING/SHOWQ (permitidas, salvo si otro tiene TX y querés SHOWQ) */
  if (!strcmp(cmd, "PING"))
  {
    dprintf(cfd, "OK\n");
    return;
  }
  if (!strcmp(cmd, "SHOWQ"))
  {
    if (tx_other_active(cfd) == 0)
    {
      dprintf(cfd, "ERR TX_ACTIVE\n");
      return;
    }
    char ev[64];
    if (kv_get(p, "evento", ev, sizeof(ev)) < 0)
    {
      dprintf(cfd, "ERR ARG\n");
      return;
    }
    char out[8192];
    if (q_show(ev, out, sizeof(out)) == 0)
      dprintf(cfd, "%s", out);
    else
      dprintf(cfd, "ERR EVENTO\n");
    return;
  }

  /* BEGIN abre TX si no hay otra; COMMIT/ROLLBACK sólo dueño */
  if (!strcmp(cmd, "BEGIN"))
  {
    if (tx_begin(cfd) == 0)
      dprintf(cfd, "OK\n");
    else
      dprintf(cfd, "ERR BUSY\n");
    return;
  }
  if (!strcmp(cmd, "COMMIT"))
  {
    if (tx_owner_only(cfd) != 0)
    {
      dprintf(cfd, "ERR NO_TX\n");
      return;
    }
    tx_end(cfd);
    dprintf(cfd, "OK\n");
    return;
  }
  if (!strcmp(cmd, "ROLLBACK"))
  {
    if (tx_owner_only(cfd) != 0)
    {
      dprintf(cfd, "ERR NO_TX\n");
      return;
    }
    tx_end(cfd);
    dprintf(cfd, "OK\n");
    return;
  }

  /* Mutaciones (ADD/UPDATE/DELETE/ATTEND/LEAVE): solo dueño si hay TX activa */
  if (tx_other_active(cfd) == 0)
  {
    dprintf(cfd, "ERR TX_ACTIVE\n");
    return;
  }
  if (tx_activa && tx_owner_only(cfd) != 0)
  {
    dprintf(cfd, "ERR NO_TX\n");
    return;
  }

  if (!strcmp(cmd, "ADD"))
  {
    rec_base_t r = {0};
    char v[64], ev[64] = "";
    if (kv_get(p, "nombre", r.nombre, sizeof(r.nombre)) < 0)
    {
      dprintf(cfd, "ERR ARG\n");
      return;
    }
    if (kv_get(p, "generador", v, sizeof(v)) > 0)
      r.generador = atoi(v);
    if (kv_get(p, "pid", v, sizeof(v)) > 0)
      r.pid = atoi(v);
    (void)kv_get(p, "evento", ev, sizeof(ev));
    if (db_add(&r, ev) == 0)
      dprintf(cfd, "OK\n");
    else
      dprintf(cfd, "ERR IO\n");
    return;
  }
  if (!strcmp(cmd, "UPDATE"))
  {
    rec_base_t patch = {0};
    char v[64];
    if (kv_get(p, "id", v, sizeof(v)) < 0)
    {
      dprintf(cfd, "ERR ARG\n");
      return;
    }
    patch.id = atoi(v);
    if (kv_get(p, "nombre", patch.nombre, sizeof(patch.nombre)) < 0)
      patch.nombre[0] = 0;
    if (kv_get(p, "generador", v, sizeof(v)) > 0)
      patch.generador = atoi(v);
    if (kv_get(p, "pid", v, sizeof(v)) > 0)
      patch.pid = atoi(v);
    if (db_update(&patch) == 0)
      dprintf(cfd, "OK\n");
    else
      dprintf(cfd, "ERR NOT_FOUND\n");
    return;
  }
  if (!strcmp(cmd, "DELETE"))
  {
    int id = 0;
    char v[32];
    if (kv_get(p, "id", v, sizeof(v)) > 0)
      id = atoi(v);
    else
      id = atoi(p);
    if (id <= 0)
    {
      dprintf(cfd, "ERR ARG\n");
      return;
    }
    if (db_delete(id) == 0)
      dprintf(cfd, "OK\n");
    else
      dprintf(cfd, "ERR NOT_FOUND\n");
    return;
  }
  if (!strcmp(cmd, "ATTEND"))
  {
    char ev[64];
    if (kv_get(p, "evento", ev, sizeof(ev)) < 0)
    {
      dprintf(cfd, "ERR ARG\n");
      return;
    }
    rec_base_t r;
    int rc = q_attend(ev, &r);
    if (rc == 0)
      dprintf(cfd, "RESULT 1\n%d,%s\nEND\n", r.id, r.nombre);
    else if (rc == -2)
      dprintf(cfd, "RESULT 0\nEND\n");
    else
      dprintf(cfd, "ERR EVENTO\n");
    return;
  }
  if (!strcmp(cmd, "LEAVE"))
  {
    int id = atoi(p);
    if (id <= 0)
    {
      dprintf(cfd, "ERR ARG\n");
      return;
    }
    if (q_leave(id) == 0)
      dprintf(cfd, "OK\n");
    else
      dprintf(cfd, "ERR NOT_FOUND\n");
    return;
  }
  if (!strcmp(cmd, "QUIT"))
  {
    dprintf(cfd, "BYE\n");
    return;
  }

  dprintf(cfd, "ERR CMD\n");
}
