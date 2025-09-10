#ifndef CSVDB_H
#define CSVDB_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <ctype.h>

#define NOMBRE_MAXLEN    64
#define TIMESTAMP_MAXLEN 20

extern int tx_active;
extern int tx_owner;
extern pthread_mutex_t tx_mtx;

/* Registro persistente en CSV (producto) */
typedef struct {
    int id;
    int generador;
    pid_t pid;
    char nombre[NOMBRE_MAXLEN];
    float precio; // [1000.00, 100000.00]
    uint32_t stock; // [0, 100]
    char timestamp[TIMESTAMP_MAXLEN]; // timestamp
    bool borrado;
} registro_t;

/*
int   id;
    char  nombre[NOMBRE_MAXLEN]; 
    float precio;   
    uint32_t stock;   
    char  timestamp[TIMESTAMP_MAXLEN]; 
    bool  borrado; 
*/

/* Operaciones sobre el archivo CSV */
int   abrir_arch(const char *csv_path);      /* abre/lee CSV */
void  cerrar_arch(void);                     /* libera memoria */
int   recargar_arch(void);                   /* recarga desde CSV */
int   guardar_arch(void);                    /* guarda a CSV */

int   buscar_id_arch(int id, registro_t *out);
int   agregar_arch(const registro_t *r);     /* agrega producto */
int   actualizar_arch(const registro_t *patch); /* modifica producto */
int   eliminar_arch(int id);                 /* borra producto */
///agregado cisco

int find_first_nombre_ci(const char *needle, registro_t *out);
/* Devuelve array con todas las coincidencias (malloc).
   *outs y *count salen seteados; el caller debe free(*outs). */
int find_all_nombres_ci(const char *needle, registro_t **outs, size_t *count);


///AGREGADO -RO
/* --- Snapshot para TX (BEGIN/COMMIT/ROLLBACK) --- */
int csvdb_begin_snapshot(void);     /* tomar snapshot in-memory */
int csvdb_commit_snapshot(void);    /* descartar snapshot */
int csvdb_rollback_snapshot(void);  /* restaurar snapshot + guardar CSV */


#endif
