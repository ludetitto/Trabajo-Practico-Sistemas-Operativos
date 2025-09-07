#ifndef CSVDB_H
#define CSVDB_H
#include <stddef.h>

#define NOMBRE_MAX  32

/* Registro base del E1 (persistente) */
typedef struct {
    int   id;
    int   generador;
    int   pid;
    char  nombre[NOMBRE_MAX];
} registro_t;

/* Nodo extendido en memoria (cola virtual) */
typedef struct nodo {
    registro_t base;          /* campos persistentes */
    char nombre[NOMBRE_MAX];
    float precio; /* [1000.00, 100000.00] */
    uint32_t stock; /* [0, 100] */
    struct nodo *ant, *sig;
} nodo_t;

/* Lista doble por evento */
typedef struct {
    nodo_t *primero, *ulttimo;
    unsigned tam;
    unsigned contador; /* para calcular posiciones nuevas */
} lista_doble_t;

/* Conjunto de colas por evento + almacenamiento lineal para búsquedas */
int   abrir_arch(const char *csv_path);      /* abre/lee CSV (E1) y crea colas por evento en memoria */
void  cerrar_arch(void);                     /* libera memoria */
int   recargar_arch(void);                    /* recarga desde CSV (descarta memoria actual) */
int   guardar_arch(void);                      /* guarda SOLO el esquema E1 (id,generador,pid,Nombre) */

int   buscar_id_arch(int id, registro_t *out);
int   agregar_arch(const registro_t *r, const char *evento_opt); /* agrega a CSV y encola (evento aleatorio si NULL) */
int   actualizar_arch(const registro_t *patch);                  /* actualiza por id base->nombre/generador/pid */
int   eliminar_arch(int id);                                   /* borra del CSV y saca de colas */

#endif
