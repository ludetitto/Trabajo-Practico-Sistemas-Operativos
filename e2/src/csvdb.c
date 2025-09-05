#define _GNU_SOURCE 1
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // strsep (con _GNU_SOURCE)
#include <strings.h> // strcasecmp
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include "../include/csvdb.h"

/* ====== Eventos soportados (coincidir con lo de E1) ====== */
static const char *EVTS[] = {
    "Lollapalooza",
    "Cosquin Rock",
    "Bresh",
    "Primavera Sound"};
#define EVT_COUNT (int)(sizeof(EVTS) / sizeof(EVTS[0]))

/* ====== Estado global ====== */
static char g_csv_path[512] = {0};
static dll_t g_queues[EVT_COUNT];
static nodo_t **g_all = NULL; /* vector de punteros a nodos (índice lineal) */
static size_t g_all_cap = 0, g_all_len = 0;

static pthread_mutex_t g_mtx = PTHREAD_MUTEX_INITIALIZER;

/* ====== Utilitarios ====== */
const char *evt_name_by_index(int idx) { return (idx >= 0 && idx < EVT_COUNT) ? EVTS[idx] : NULL; }
int evt_index(const char *e)
{
  if (!e)
    return -1;
  for (int i = 0; i < EVT_COUNT; i++)
    if (strcasecmp(e, EVTS[i]) == 0)
      return i;
  return -1;
}
const char *evt_random(void)
{
  static int seeded = 0;
  if (!seeded)
  {
    srand((unsigned)time(NULL) ^ (unsigned)clock());
    seeded = 1;
  }
  return EVTS[rand() % EVT_COUNT];
}

static void list_push_back(dll_t *L, nodo_t *n)
{
  n->prev = L->tail;
  n->next = NULL;
  if (L->tail)
    L->tail->next = n;
  else
    L->head = n;
  L->tail = n;
  L->len++;
}
static void list_remove(dll_t *L, nodo_t *n)
{
  if (n->prev)
    n->prev->next = n->next;
  else
    L->head = n->next;
  if (n->next)
    n->next->prev = n->prev;
  else
    L->tail = n->prev;
  L->len--;
}
static void all_push(nodo_t *n)
{
  if (g_all_len == g_all_cap)
  {
    size_t nc = g_all_cap ? g_all_cap * 2 : 128;
    nodo_t **nv = (nodo_t **)realloc(g_all, nc * sizeof(nodo_t *));
    if (!nv)
      return;
    g_all = nv;
    g_all_cap = nc;
  }
  g_all[g_all_len++] = n;
}
static nodo_t *find_by_id(int id)
{
  for (size_t i = 0; i < g_all_len; i++)
    if (g_all[i]->base.id == id)
      return g_all[i];
  return NULL;
}

/* ====== Lectura/Escritura de CSV (E1 schema) ======
   Formato: id,generador,pid,Nombre
*/
static int parse_line_base(const char *s, rec_base_t *r)
{
  char tmp[256];
  strncpy(tmp, s, sizeof(tmp) - 1);
  tmp[sizeof(tmp) - 1] = 0;
  char *p = tmp, *tok;
  tok = strsep(&p, ",\r\n");
  if (!tok)
    return -1;
  r->id = atoi(tok);
  tok = strsep(&p, ",\r\n");
  if (!tok)
    return -1;
  r->generador = atoi(tok);
  tok = strsep(&p, ",\r\n");
  if (!tok)
    return -1;
  r->pid = atoi(tok);
  tok = strsep(&p, ",\r\n");
  if (!tok)
    return -1;
  strncpy(r->nombre, tok, NAME_MAXLEN - 1);
  r->nombre[NAME_MAXLEN - 1] = 0;
  return 0;
}

static int save_all_base(FILE *f)
{
  fprintf(f, "id,generador,pid,Nombre\n");
  for (size_t i = 0; i < g_all_len; i++)
  {
    rec_base_t *b = &g_all[i]->base;
    fprintf(f, "%d,%d,%d,%s\n", b->id, b->generador, b->pid, b->nombre);
  }
  return 0;
}

/* ====== Carga desde CSV del E1 y construcción de colas ====== */
int db_open(const char *csv_path)
{
  pthread_mutex_lock(&g_mtx);
  strncpy(g_csv_path, csv_path, sizeof(g_csv_path) - 1);
  for (int i = 0; i < EVT_COUNT; i++)
  {
    g_queues[i].head = g_queues[i].tail = NULL;
    g_queues[i].len = 0;
    g_queues[i].contador = 0;
  }
  free(g_all);
  g_all = NULL;
  g_all_cap = g_all_len = 0;

  FILE *f = fopen(g_csv_path, "r");
  if (!f)
  {
    pthread_mutex_unlock(&g_mtx);
    return -1;
  }

  char line[512];
  if (!fgets(line, sizeof(line), f))
  {
    fclose(f);
    pthread_mutex_unlock(&g_mtx);
    return -1;
  }
  if (strncasecmp(line, "id,generador,pid,Nombre", 23) != 0)
  {
    rec_base_t r;
    if (parse_line_base(line, &r) == 0)
    {
      nodo_t *n = (nodo_t *)calloc(1, sizeof(nodo_t));
      n->base = r;
      strncpy(n->estado, "Esperando", EST_MAXLEN - 1);
      strncpy(n->evento, evt_random(), EVT_MAXLEN - 1);
      int ei = evt_index(n->evento);
      if (ei < 0)
        ei = 0;
      n->posicion = ++g_queues[ei].contador;
      list_push_back(&g_queues[ei], n);
      all_push(n);
    }
  }
  while (fgets(line, sizeof(line), f))
  {
    rec_base_t r;
    if (parse_line_base(line, &r) != 0)
      continue;
    nodo_t *n = (nodo_t *)calloc(1, sizeof(nodo_t));
    n->base = r;
    strncpy(n->estado, "Esperando", EST_MAXLEN - 1);
    strncpy(n->evento, evt_random(), EVT_MAXLEN - 1);
    int ei = evt_index(n->evento);
    if (ei < 0)
      ei = 0;
    n->posicion = ++g_queues[ei].contador;
    list_push_back(&g_queues[ei], n);
    all_push(n);
  }
  fclose(f);
  pthread_mutex_unlock(&g_mtx);
  return 0;
}

void db_close(void)
{
  pthread_mutex_lock(&g_mtx);
  for (size_t i = 0; i < g_all_len; i++)
    free(g_all[i]);
  free(g_all);
  g_all = NULL;
  g_all_cap = g_all_len = 0;
  for (int i = 0; i < EVT_COUNT; i++)
  {
    g_queues[i].head = g_queues[i].tail = NULL;
    g_queues[i].len = 0;
    g_queues[i].contador = 0;
  }
  pthread_mutex_unlock(&g_mtx);
}

int db_reload(void)
{
  db_close();
  return db_open(g_csv_path);
}

int db_save(void)
{
  pthread_mutex_lock(&g_mtx);
  FILE *f = fopen(g_csv_path, "w");
  if (!f)
  {
    pthread_mutex_unlock(&g_mtx);
    return -1;
  }
  save_all_base(f);
  fclose(f);
  pthread_mutex_unlock(&g_mtx);
  return 0;
}

/* ====== CRUD base ====== */
int db_find_id(int id, rec_base_t *out)
{
  pthread_mutex_lock(&g_mtx);
  nodo_t *n = find_by_id(id);
  if (!n)
  {
    pthread_mutex_unlock(&g_mtx);
    return -1;
  }
  if (out)
    *out = n->base;
  pthread_mutex_unlock(&g_mtx);
  return 0;
}

int db_add(const rec_base_t *r, const char *evento_opt)
{
  if (!r)
    return -1;
  pthread_mutex_lock(&g_mtx);
  int maxid = 0;
  for (size_t i = 0; i < g_all_len; i++)
    if (g_all[i]->base.id > maxid)
      maxid = g_all[i]->base.id;
  nodo_t *n = (nodo_t *)calloc(1, sizeof(nodo_t));
  n->base = *r;
  if (n->base.id == 0)
    n->base.id = maxid + 1;
  const char *ev = (evento_opt && *evento_opt) ? evento_opt : evt_random();
  strncpy(n->evento, ev, EVT_MAXLEN - 1);
  strncpy(n->estado, "Esperando", EST_MAXLEN - 1);
  int ei = evt_index(n->evento);
  if (ei < 0)
    ei = 0;
  n->posicion = ++g_queues[ei].contador;
  list_push_back(&g_queues[ei], n);
  all_push(n);
  int rc = db_save();
  pthread_mutex_unlock(&g_mtx);
  return rc;
}

int db_update(const rec_base_t *patch)
{
  if (!patch)
    return -1;
  pthread_mutex_lock(&g_mtx);
  nodo_t *n = find_by_id(patch->id);
  if (!n)
  {
    pthread_mutex_unlock(&g_mtx);
    return -1;
  }
  if (patch->nombre[0])
    snprintf(n->base.nombre, NAME_MAXLEN, "%s", patch->nombre);
  if (patch->generador)
    n->base.generador = patch->generador;
  if (patch->pid)
    n->base.pid = patch->pid;
  int rc = db_save();
  pthread_mutex_unlock(&g_mtx);
  return rc;
}

int db_delete(int id)
{
  pthread_mutex_lock(&g_mtx);
  nodo_t *n = find_by_id(id);
  if (!n)
  {
    pthread_mutex_unlock(&g_mtx);
    return -1;
  }
  int ei = evt_index(n->evento);
  if (ei < 0)
    ei = 0;
  list_remove(&g_queues[ei], n);
  for (size_t i = 0; i < g_all_len; i++)
    if (g_all[i] == n)
    {
      g_all[i] = g_all[g_all_len - 1];
      g_all_len--;
      break;
    }
  free(n);
  int rc = db_save();
  pthread_mutex_unlock(&g_mtx);
  return rc;
}

/* ====== Operaciones de cola ====== */
int q_attend(const char *evento, rec_base_t *out)
{
  pthread_mutex_lock(&g_mtx);
  int ei = evt_index(evento);
  if (ei < 0)
  {
    pthread_mutex_unlock(&g_mtx);
    return -1;
  }
  nodo_t *n = g_queues[ei].head;
  if (!n)
  {
    pthread_mutex_unlock(&g_mtx);
    return -2;
  } /* cola vacía */
  list_remove(&g_queues[ei], n);
  /* remover del índice global y devolver */
  for (size_t i = 0; i < g_all_len; i++)
    if (g_all[i] == n)
    {
      g_all[i] = g_all[g_all_len - 1];
      g_all_len--;
      break;
    }
  if (out)
    *out = n->base;
  free(n);
  pthread_mutex_unlock(&g_mtx);
  return 0;
}

int q_leave(int id)
{
  pthread_mutex_lock(&g_mtx);
  nodo_t *n = find_by_id(id);
  if (!n)
  {
    pthread_mutex_unlock(&g_mtx);
    return -1;
  }
  int ei = evt_index(n->evento);
  if (ei < 0)
    ei = 0;
  list_remove(&g_queues[ei], n);
  for (size_t i = 0; i < g_all_len; i++)
    if (g_all[i] == n)
    {
      g_all[i] = g_all[g_all_len - 1];
      g_all_len--;
      break;
    }
  free(n);
  pthread_mutex_unlock(&g_mtx);
  return 0;
}

int q_show(const char *evento, char *buf, size_t bufsz)
{
  if (!buf || bufsz < 4)
    return -1;
  pthread_mutex_lock(&g_mtx);
  int ei = evt_index(evento);
  if (ei < 0)
  {
    pthread_mutex_unlock(&g_mtx);
    return -1;
  }
  size_t off = 0;
  off += (size_t)snprintf(buf + off, bufsz - off, "RESULT %u\n", g_queues[ei].len);
  for (nodo_t *p = g_queues[ei].head; p && off + 64 < bufsz; p = p->next)
  {
    off += (size_t)snprintf(buf + off, bufsz - off, "%d,%s,%s,%u\n",
                            p->base.id, p->base.nombre, "Esperando", p->posicion);
  }
  off += (size_t)snprintf(buf + off, bufsz - off, "END\n");
  pthread_mutex_unlock(&g_mtx);
  return 0;
}
