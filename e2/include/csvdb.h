#ifndef CSVDB_H
#define CSVDB_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#define NOMBRE_MAX      64
#define TIMESTAMP_MAXLEN 20

/* Registro persistente en CSV (producto) */
typedef struct {
    int   id;                      /* ID único */
    char  nombre[NOMBRE_MAX];      /* nombre del producto */
    float precio;                  /* precio del producto */
    uint32_t stock;                /* cantidad en stock */
    char  timestamp[TIMESTAMP_MAXLEN]; /* fecha/hora de última modificación */
    bool  borrado;                 /* marca lógica de borrado */
} registro_t;

/* Operaciones sobre el archivo CSV */
int   abrir_arch(const char *csv_path);      /* abre/lee CSV */
void  cerrar_arch(void);                     /* libera memoria */
int   recargar_arch(void);                   /* recarga desde CSV */
int   guardar_arch(void);                    /* guarda a CSV */

int   buscar_id_arch(int id, registro_t *out);
int   agregar_arch(const registro_t *r);     /* agrega producto */
int   actualizar_arch(const registro_t *patch); /* modifica producto */
int   eliminar_arch(int id);                 /* borra producto */

#endif
