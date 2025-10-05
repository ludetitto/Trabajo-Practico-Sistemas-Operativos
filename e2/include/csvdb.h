#ifndef CSVDB_H
#define CSVDB_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>   // pid_t
#include <pthread.h>
#include <ctype.h>

#define NOMBRE_MAXLEN    64
#define TIMESTAMP_MAXLEN 20

/* Estado de transacción (coordinado externamente) */
extern int tx_active;
extern int tx_owner;
extern pthread_mutex_t tx_mtx;

/* Registro persistente en CSV (producto) */
typedef struct {
    int      id;
    int      generador;
    pid_t    pid;
    char     nombre[NOMBRE_MAXLEN];
    float    precio;                    // [1000.00, 100000.00]
    uint32_t stock;                     // [0, 100]
    char     timestamp[TIMESTAMP_MAXLEN];
    bool     borrado;
} registro_t;

/* ----------------- Operaciones sobre el archivo CSV ----------------- */
int  abrir_arch(const char *csv_path);          /* abre/lee CSV a memoria */
void cerrar_arch(void);                          /* libera memoria */
int  recargar_arch(void);                        /* recarga desde CSV */
int  guardar_arch(void);                         /* guarda memoria -> CSV */

int  buscar_id_arch(int id, registro_t *out);
int  agregar_arch(const registro_t *r);          /* agrega producto */
int  actualizar_arch(const registro_t *patch);   /* modifica producto */
int  eliminar_arch(int id);                      /* borra producto (lógico) */

/* Búsqueda por nombre */
int  buscar_nombre_primero(const char *buscado, registro_t *out);
/* Devuelve array con todas las coincidencias (malloc).
   *outs y *cont salen seteados; el caller debe free(*outs). */
int  buscar_nombre_todos(const char *buscado, registro_t **outs, size_t *cont);

/* Modificaciones por ID (agregado por Tavo) */
int  modificar_nombre_id(int id, const char *nombreNuevo, registro_t *out);
int  modificar_precio_id(int id, float precioNuevo, registro_t *out);
int  modificar_stock_id(int id, uint32_t stockNuevo, registro_t *out);

/* ----------------- Snapshot / Transacciones -----------------
   generar_snapshot()  -> BEGIN: toma snapshot in-memory
   guardar_snapshot()  -> COMMIT: persiste cambios y libera snapshot
   descartar_snapshot()-> ROLLBACK: restaura snapshot y descarta cambios
---------------------------------------------------------------- */
int  generar_snapshot(void);
int  guardar_snapshot(void);
int  descartar_snapshot(void);

#endif /* CSVDB_H */
