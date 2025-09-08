#ifndef CSVDB_H
#define CSVDB_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#define NOMBRE_MAXLEN    64
#define TIMESTAMP_MAXLEN 20

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

#endif
