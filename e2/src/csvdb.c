#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include "../include/csvdb.h"

/* ====== Estado global ====== */
static char csv_path[512] = {0};
static registro_t *productos = NULL;
static size_t productos_tam = 0;
static size_t productos_cap = 0;

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

/* ====== Internos ====== */
static void asegurar_capacidad(void) 
{
    if (productos_tam == productos_cap) {
        size_t nueva_cap = productos_cap ? productos_cap * 2 : 128;
        registro_t *tmp = realloc(productos, nueva_cap * sizeof(registro_t));
        if (!tmp) return;
        productos = tmp;
        productos_cap = nueva_cap;
    }
}

static int parsear_linea(const char *linea, registro_t *r) 
{
    return sscanf(linea, "%d,%d,%d,%63[^,],%f,%u,%19[^,],%d",
                  &r->id, &r->generador, &r->pid, r->nombre,
                  &r->precio, &r->stock, r->timestamp, (int*)&r->borrado) == 8 ? 0 : -1;
}

static void guardar_todos(FILE *f) 
{
    fprintf(f, "id,nombre,precio,stock,timestamp,borrado\n");
    for (size_t i = 0; i < productos_tam; i++) 
    {
        registro_t *r = &productos[i];
        fprintf(f, "%d,%d,%d,%s,%f,%u,%s,%d\n",
                  r->id, r->generador, r->pid, r->nombre,
                  r->precio, r->stock, r->timestamp, r->borrado? 1: 0);
    }
}

/* ====== API pública ====== */
int abrir_arch(const char *path) 
{
    FILE *f;
    char linea[512];

    pthread_mutex_lock(&mtx);
    strncpy(csv_path, path, sizeof(csv_path)-1);

    free(productos);
    productos = NULL;
    productos_tam = productos_cap = 0;

    f = fopen(path, "r");
    if (!f) 
    { 
        pthread_mutex_unlock(&mtx); 
        return -1; 
    }

    if (!fgets(linea, sizeof(linea), f)) 
    { 
        fclose(f); 
        pthread_mutex_unlock(&mtx); 
        return -1; 
    } // header

    while (fgets(linea, sizeof(linea), f)) 
    {
        registro_t r = {0};
        if (!parsear_linea(linea, &r)) 
        {
            asegurar_capacidad();
            productos[productos_tam++] = r;
        }
    }

    fclose(f);
    pthread_mutex_unlock(&mtx);
    return 0;
}

void cerrar_arch(void) 
{
    pthread_mutex_lock(&mtx);
    free(productos);
    productos = NULL;
    productos_tam = productos_cap = 0;
    pthread_mutex_unlock(&mtx);
}

int recargar_arch(void) 
{
    return abrir_arch(csv_path);
}

int guardar_arch(void) 
{
    FILE *f;
    pthread_mutex_lock(&mtx);
    f = fopen(csv_path, "w");
    if (!f) 
    { 
        pthread_mutex_unlock(&mtx); 
        return -1; 
    }
    guardar_todos(f);
    fclose(f);
    pthread_mutex_unlock(&mtx);
    return 0;
}

/* ===== CRUD ===== */
int buscar_id_arch(int id, registro_t *out) 
{
    pthread_mutex_lock(&mtx);
    for (size_t i = 0; i < productos_tam; i++) 
    {
        if (productos[i].id == id && !productos[i].borrado) 
        {
            if (out) *out = productos[i];
            pthread_mutex_unlock(&mtx);
            return 0;
        }
    }
    pthread_mutex_unlock(&mtx);
    return -1;
}

int agregar_arch(const registro_t *r) 
{
    int maxid = 0, rc;

    if (!r) 
        return -1;
    
    pthread_mutex_lock(&mtx);
    for (size_t i = 0; i < productos_tam; i++)
        if (productos[i].id > maxid) maxid = productos[i].id;

    registro_t nuevo = *r;
    if (!nuevo.id) 
        nuevo.id = maxid + 1;

    asegurar_capacidad();
    productos[productos_tam++] = nuevo;

    rc = guardar_arch();
    pthread_mutex_unlock(&mtx);
    return rc;
}

int actualizar_arch(const registro_t *patch) 
{
    int rc;
    
    if (!patch) 
        return -1;
    pthread_mutex_lock(&mtx);

    for (size_t i = 0; i < productos_tam; i++) 
    {
        if (productos[i].id == patch->id) 
        {
            if (patch->nombre[0]) 
            {
                strncpy(productos[i].nombre, patch->nombre, NOMBRE_MAXLEN - 1);
                productos[i].nombre[NOMBRE_MAXLEN - 1] = '\0';
            }
            if (patch->precio > 0) 
                productos[i].precio = patch->precio;
            // if (patch->stock >= 0)
            productos[i].stock = patch->stock;
            rc = guardar_arch();
            pthread_mutex_unlock(&mtx);
            return rc;
        }
    }
    pthread_mutex_unlock(&mtx);
    return -1;
}

int eliminar_arch(int id) 
{
    pthread_mutex_lock(&mtx);
    for (size_t i = 0; i < productos_tam; i++) 
    {
        if (productos[i].id == id) 
        {
            productos[i].borrado = true;
            pthread_mutex_unlock(&mtx);
            int rc = guardar_arch();
            return rc;
        }
    }
    pthread_mutex_unlock(&mtx);
    return -1;
}

///AGREGADO -RO
/* ==== Snapshot de TX ====
Supone que ya existen en este archivo: 
   - registro_t *productos;
   - size_t productos_tam, productos_cap;
   - pthread_mutex_t mtx;
   - int guardar_arch(void);   // reescribe CSV desde 'productos[]'
*/
static registro_t *snapshot      = NULL;
static size_t      snapshot_tam  = 0;
static size_t      snapshot_cap  = 0;
static int         snapshot_on   = 0;

/* Copia en memoria el vector productos[] tal como está al momento del BEGIN */
int csvdb_begin_snapshot(void) {
    pthread_mutex_lock(&mtx);
    if (snapshot_on) {                 // ya hay snapshot
        pthread_mutex_unlock(&mtx);
        return -1;
    }
    snapshot_cap = productos_cap;
    snapshot_tam = productos_tam;
    snapshot = (registro_t*)malloc(sizeof(registro_t) * (snapshot_cap ? snapshot_cap : 1));
    if (!snapshot) {
        pthread_mutex_unlock(&mtx);
        return -1;
    }
    if (productos_tam) {
        memcpy(snapshot, productos, sizeof(registro_t) * productos_tam);
    }
    snapshot_on = 1;
    pthread_mutex_unlock(&mtx);
    return 0;
}

/* Descarta el snapshot (se usa en COMMIT) */
int csvdb_commit_snapshot(void) {
    pthread_mutex_lock(&mtx);
    if (snapshot_on) {
        free(snapshot);
        snapshot = NULL;
        snapshot_tam = snapshot_cap = 0;
        snapshot_on = 0;
    }
    pthread_mutex_unlock(&mtx);
    return 0;
}

/* Restaura el snapshot y persiste a CSV (se usa en ROLLBACK) */
int csvdb_rollback_snapshot(void) {
    pthread_mutex_lock(&mtx);
    if (!snapshot_on) {
        pthread_mutex_unlock(&mtx);
        return -1;
    }

    /* Asegurar capacidad en productos[] */
    if (productos_cap < snapshot_cap) {
        registro_t *tmp = (registro_t*)realloc(productos, sizeof(registro_t) * snapshot_cap);
        if (!tmp) {   // si no podemos ampliar, al menos intentamos con el tamaño justo
            tmp = (registro_t*)realloc(productos, sizeof(registro_t) * snapshot_tam);
            if (!tmp) {
                pthread_mutex_unlock(&mtx);
                return -1;
            }
            productos_cap = snapshot_tam;
        } else {
            productos_cap = snapshot_cap;
        }
        productos = tmp;
    }

    /* Restaurar contenido y tamaño */
    if (snapshot_tam) {
        memcpy(productos, snapshot, sizeof(registro_t) * snapshot_tam);
    }
    productos_tam = snapshot_tam;

    /* Liberar snapshot */
    free(snapshot);
    snapshot = NULL;
    snapshot_tam = snapshot_cap = 0;
    snapshot_on = 0;

    pthread_mutex_unlock(&mtx);

    /* Persistir estado restaurado (usa el CSV actual y regraba todo) */
    return guardar_arch();
}
