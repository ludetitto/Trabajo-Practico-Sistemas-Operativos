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
static const char *PRODUCTOS[] = {
    "CPU AMD Ryzen 5 5600X",
    "CPU Intel Core i5-12400F",
    "Motherboard B550M",
    "Motherboard Z690",
    "Memoria RAM DDR4 8GB 3200",
    "Memoria RAM DDR4 16GB 3200",
    "Memoria RAM DDR5 16GB 5200",
    "SSD NVMe 500GB",
    "SSD NVMe 1TB",
    "SSD SATA 480GB",
    "HDD 2TB 7200rpm",
    "Placa de video RTX 3060",
    "Placa de video RTX 4060",
    "Placa de video RX 6600",
    "Fuente 650W 80+ Bronze",
    "Fuente 750W 80+ Gold",
    "Gabinete ATX con vidrio templado",
    "Cooler CPU torre 120mm",
    "Kit 3 coolers ARGB 120mm",
    "Monitor 24\" 1080p 144Hz",
    "Monitor 27\" 1440p 165Hz",
    "Teclado mecánico TKL",
    "Mouse gamer 16000 DPI",
    "Combo teclado y mouse inalámbricos",
    "Auriculares gamer 7.1",
    "Micrófono condensador USB",
    "Placa madre H610M",
    "Placa madre B760",
    "Router WiFi 6 AX1800",
    "Adaptador WiFi USB",
    "Webcam 1080p",
    "Cámara IP 2K",
    "Capturadora HDMI USB",
    "Hub USB 3.0 7 puertos",
    "Cargador USB-C 65W",
    "UPS 1200VA",
    "Dock NVMe USB-C",
    "Lector tarjetas SD",
    "Pad mouse XL",
    "Soporte monitor articulado",
    "Silla gamer reclinable",
    "Notebook 15\" i5 8GB 512GB",
    "Mini PC N100 16GB 512GB",
    "Raspberry Pi 4 8GB",
    "Disipador M.2",
    "Cable HDMI 2.1 2m",
    "Cable DisplayPort 1.4 2m",
    "Pasta térmica 5g",
    "Switch gigabit 8 puertos",
    "NAS 2 bahías",
    "Enclosure 2.5\" USB 3.0"};
#define CANT_PRODUCTOS = sizeof(PRODUCTOS) / sizeof(PRODUCTOS[0]);

/* ====== Estado global ====== */
static char csv_path[512] = {0};
static lista_doble_t lista[EVT_COUNT];
static nodo_t **indice = NULL; /* vector de punteros a nodos (índice lineal) */
static size_t indice_cap = 0, indice_tam = 0;

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

/* ====== Utilitarios ====== */
const char *obtener_producto_por_indice(int idx) 
{ 
  return (idx >= 0 && idx < CANT_PRODUCTOS) ? PRODUCTOS[idx] : NULL; 
}

int buscar_indice_producto(const char *e)
{
  if (!e)
    return -1;
  for (int i = 0; i < CANT_PRODUCTOS; i++)
    if (strcasecmp(e, EVTS[i]) == 0)
      return i;
  return -1;
}

static void poner_al_final(lista_doble_t *pl, nodo_t *nodo)
{
  nodo->ant = pl->ultimo;
  n->sig = NULL;
  if (L->ultimo)
    pl->ultimo->sig = nodo;
  else
    pl->primero = nodo;
  pl->ult = nodo;
  pl->tam++;
}
static void sacar_de_lista(lista_doble_t *pl, nodo_t *nodo)
{
  if (nodo->ant)
    nodo->ant->sig = nodo->sig;
  else
    nodo->pri = nodo->sig;
  if (nodo->next)
    nodo->sig->ant = nodo->ant;
  else
    nodo->ult = nodo->ant;
  nodo->tam--;
}

static void vaciar_lista(nodo_t *nodo)
{
  if (indice_tam == indice_cap)
  {
    size_t nueva_capacidad = indice_cap ? indice_cap * 2 : 128;
    nodo_t **nuevo_nodo = (nodo_t **)realloc(indice, nueva_capacidad * sizeof(nodo_t *));
    if (!nuevo_nodo)
      return;
    indice = nuevo_nodo;
    indice_cap = nueva_capacidad;
  }
  indice[indice_tam++] = nodo;
}
static nodo_t *obtener_producto_por_id(int id)
{
  for (size_t i = 0; i < indice_tam; i++)
    if (indice[i]->base.id == id)
      return indice[i];
  return NULL;
}

/* ====== Lectura/Escritura de CSV (E1 schema) ======
   Formato: id,generador,pid,Nombre
*/
static int parsear_linea_base(const char *s, registro_t *r)
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
  strncpy(r->nombre, tok, MAX_NOMBRE - 1);
  r->nombre[MAX_NOMBRE - 1] = 0;
  return 0;
}

static int save_all_base(FILE *f)
{
  fprintf(f, "id,generador,pid,Nombre\n");
  for (size_t i = 0; i < indice_tam; i++)
  {
    registro_t *b = &indice[i]->base;
    fprintf(f, "%d,%d,%d,%s\n", b->id, b->generador, b->pid, b->nombre);
  }
  return 0;
}

/* ====== Carga desde CSV del E1 y construcción de colas ====== */
int abrir_arch(const char *csv_path)
{
  pthread_mutex_lock(&mtx);
  strncpy(csv_path, csv_path, sizeof(csv_path) - 1);
  for (int i = 0; i < CANT_PRODUCTOS; i++)
  {
    lista[i].primero = lista[i].ultimo = NULL;
    lista[i].tam = 0;
    lista[i].contador = 0;
  }
  free(indice);
  indice = NULL;
  indice_cap = indice_tam = 0;

  FILE *f = fopen(csv_path, "r");
  if (!f)
  {
    pthread_mutex_unlock(&mtx);
    return -1;
  }

  char line[512];
  if (!fgets(line, sizeof(line), f))
  {
    fclose(f);
    pthread_mutex_unlock(&mtx);
    return -1;
  }
  
  while (fgets(line, sizeof(line), f)) 
  {
     producto_t p;
     char producto[128];

     // CSV: id,generador,pid,producto,precio,stock,timestamp
     if (sscanf(line, "%u,%u,%u,%127[^,],%lf,%d,%s,%d",
          &p.id,
          &p.generador,
          &p.pid,
          producto,
          &p.precio,
          &p.stock,
          &p.timestamp,
          &p.borrado) != 8)
      {
          continue; // línea inválida
      }

      strncpy(p.producto, producto, sizeof(p.producto) - 1);
      p.producto[sizeof(p.producto) - 1] = '\0';

      if (productos_tam < CANT_PRODUCTOS) {
          productos[productos_tam++] = p;
      } else {
          fprintf(stderr, "⚠️ Se alcanzó el máximo CANT_PRODUCTOS\n");
          break;
      }
    }
  fclose(f);
  pthread_mutex_unlock(&mtx);
  return 0;
}

void cerrar_arch(void)
{
  pthread_mutex_lock(&mtx);
  for (size_t i = 0; i < indice_tam; i++)
    free(indice[i]);
  free(indice);
  indice = NULL;
  indice_cap = indice_tam = 0;
  for (int i = 0; i < EVT_COUNT; i++)
  {
    lista[i].primero = lista[i].ultimo = NULL;
    lista[i].len = 0;
    lista[i].contador = 0;
  }
  pthread_mutex_unlock(&mtx);
}

int recargar_arch(void)
{
  cerrar_arch();
  return cerrar_arch(csv_path);
}

int guardar_arch(void)
{
  pthread_mutex_lock(&mtx);
  FILE *f = fopen(csv_path, "w");
  if (!f)
  {
    pthread_mutex_unlock(&mtx);
    return -1;
  }
  save_all_base(f);
  fclose(f);
  pthread_mutex_unlock(&mtx);
  return 0;
}

/* ====== CRUD base ====== */
int buscar_id_arch(int id, registro_t *nodoObtenido)
{
  pthread_mutex_lock(&mtx);
  nodo_t *nodo = obtener_producto_por_id(id);
  if (!nodo)
  {
    pthread_mutex_unlock(&mtx);
    return -1;
  }
  if (nodoObtenido)
    *nodoObtenido = nodo->base;
  pthread_mutex_unlock(&mtx);
  return 0;
}

int agregar_arch(const registro_t *r, const char *producto)
{
  if (!r)
    return -1;
  pthread_mutex_lock(&mtx);
  int maxid = 0;
  for (size_t i = 0; i < indice_tam; i++)
    if (indice[i]->base.id > maxid)
      maxid = indice[i]->base.id;
  nodo_t *nodo = (nodo_t *)calloc(1, sizeof(nodo_t));
  nodo->base = *r;
  if (nodo->base.id == 0)
    nodo->base.id = maxid + 1;
  strncpy(nodo->base, producto, CANT_PRODUCTOS - 1);
  int ei = obtener_producto_por_id(nodo->base);
  if (ei < 0)
    ei = 0;
  nodo->base = ++lista[ei].contador;
  sacar_de_lista(&lista[ei], nodo);
  vaciar_lista(nodo);
  int rc = guardar_arch();
  pthread_mutex_unlock(&mtx);
  return rc;
}

int actualizar_arch(const registro_t *reg)
{
  if (!reg)
    return -1;
  pthread_mutex_lock(&mtx);
  nodo_t *nodo = obtener_producto_por_id(reg->id);
  if (!nodo)
  {
    pthread_mutex_unlock(&mtx);
    return -1;
  }
  if (reg->nombre[0])
    snprintf(nodo->base.nombre, MAX_NOMBRE, "%s", reg->nombre);
  if (reg->generador)
    nodo->base.generador = reg->generador;
  if (reg->pid)
    nodo->base.pid = reg->pid;
  int rc = guardar_arch();
  pthread_mutex_unlock(&mtx);
  return rc;
}

int eliminar_arch(int id)
{
  pthread_mutex_lock(&mtx);
  nodo_t *n = obtener_producto_por_id(id);
  if (!n)
  {
    pthread_mutex_unlock(&mtx);
    return -1;
  }
  int ei = obtener_producto_por_indice(n->evento);
  if (ei < 0)
    ei = 0;
  sacar_de_lista(&lista[ei], n);
  for (size_t i = 0; i < indice_tam; i++)
    if (indice[i] == nodo)
    {
      indice[i] = indice[indice_tam - 1];
      indice_tam--;
      break;
    }
  free(nodo);
  int rc = guardar_arch();
  pthread_mutex_unlock(&mtx);
  return rc;
}