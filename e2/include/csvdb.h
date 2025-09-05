#ifndef CSVDB_H
#define CSVDB_H
#include <stddef.h>

#define NAME_MAXLEN  32
#define EVT_MAXLEN   32
#define EST_MAXLEN   16

/* Registro base del E1 (persistente) */
typedef struct {
    int   id;
    int   generador;
    int   pid;
    char  nombre[NAME_MAXLEN];
} rec_base_t;

/* Nodo extendido en memoria (cola virtual) */
typedef struct nodo {
    rec_base_t base;          /* campos persistentes */
    char evento[EVT_MAXLEN];  /* "Lollapalooza", "Cosquin Rock", "Bresh", "Primavera Sound" */
    char estado[EST_MAXLEN];  /* "Esperando","Atendido","Cancelado" */
    unsigned posicion;        /* posición dentro de su evento */
    struct nodo *prev, *next;
} nodo_t;

/* Lista doble por evento */
typedef struct {
    nodo_t *head, *tail;
    unsigned len;
    unsigned contador; /* para calcular posiciones nuevas */
} dll_t;

/* Conjunto de colas por evento + almacenamiento lineal para búsquedas */
int   db_open(const char *csv_path);      /* abre/lee CSV (E1) y crea colas por evento en memoria */
void  db_close(void);                     /* libera memoria */
int   db_reload(void);                    /* recarga desde CSV (descarta memoria actual) */
int   db_save(void);                      /* guarda SOLO el esquema E1 (id,generador,pid,Nombre) */

int   db_find_id(int id, rec_base_t *out);
int   db_add(const rec_base_t *r, const char *evento_opt); /* agrega a CSV y encola (evento aleatorio si NULL) */
int   db_update(const rec_base_t *patch);                  /* actualiza por id base->nombre/generador/pid */
int   db_delete(int id);                                   /* borra del CSV y saca de colas */

int   q_attend(const char *evento, rec_base_t *out);       /* atiende primero de un evento (cambia estado) */
int   q_leave(int id);                                     /* alguien abandona la cola (Cancelado) */
int   q_show(const char *evento, char *buf, size_t bufsz); /* lista texto de la cola del evento */

const char* evt_random(void);
int   evt_index(const char *e);            /* -1 si no existe */
const char* evt_name_by_index(int idx);    /* nombre por índice */

#endif
