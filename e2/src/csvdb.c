// csvdb.c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include "../include/csvdb.h"

/* ==== Snapshot de TX (BEGIN/COMMIT/ROLLBACK) ==== */
static registro_t *snapshot      = NULL;
static size_t      snapshot_tam  = 0;
static size_t      snapshot_cap  = 0;
static int         snapshot_on   = 0;

/* ====== Estado global (BD en memoria) ====== */
static char csv_path[512] = {0};
static registro_t *productos = NULL;
static size_t productos_tam = 0;
static size_t productos_cap = 0;
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;    // protege productos[]

/* ====== Helpers internos ====== */
static void asegurar_capacidad(void) 
{
    if (productos_tam == productos_cap) 
    {
        size_t nueva_cap = productos_cap ? productos_cap * 2 : 128;
        registro_t *tmp = (registro_t*)realloc(productos, nueva_cap * sizeof(registro_t));
        if (!tmp) return;
        productos = tmp;
        productos_cap = nueva_cap;
    }
}

/* compara subcadena case-insensitive: true si hay needle dentro de haystack */
static int buscar_cadena(const char *nombre_producto, const char *buscado) 
{
    if (!buscado || !*buscado) 
        return 1;
    if (!nombre_producto) 
        return 0;
    // búsqueda simple O(n*m) en minúsculas
    for (const char *h = nombre_producto; *h; ++h) 
    {
        const char *p = h, *q = buscado;
        while (*p && *q) 
        {
            char c1 = (char)tolower((unsigned char)*p);
            char c2 = (char)tolower((unsigned char)*q);
            if (c1 != c2) 
                break;
            ++p; ++q;
        }
        if (!*q) 
            return 1;
    }
    return 0;
}

static int parsear_linea(const char *linea, registro_t *r) {
    // soporte para dos formatos: antiguo (6 cols):
    //   id,generador,pid,nombre,precio,stock
    // y nuevo (8 cols):
    //   id,generador,pid,nombre,precio,stock,timestamp,borrado
    int matched = sscanf(linea, "%d,%d,%d,%63[^,],%f,%u,%19[^,],%d",
                  &r->id, &r->generador, &r->pid, r->nombre,
                  &r->precio, &r->stock, r->timestamp, (int*)&r->borrado);
    if (matched == 8) return 0;
    if (matched == 6) {
        /* rellenar campos faltantes */
        r->timestamp[0] = '\0';
        r->borrado = 0;
        return 0;
    }
    return -1;
}

/* Header consistente (8 columnas) + filas */
static void guardar_todos(FILE *f) {
    fprintf(f, "id,generador,pid,nombre,precio,stock,timestamp,borrado\n");
    for (size_t i = 0; i < productos_tam; i++) {
        registro_t *r = &productos[i];
        fprintf(f, "%d,%d,%d,%s,%.2f,%u,%s,%d\n",
                r->id, r->generador, r->pid, r->nombre,
                r->precio, r->stock, r->timestamp, r->borrado ? 1 : 0);
    }
}

/* ====== Guardado en CSV ====== */
static int guardar_arch_locked(void) {        // NO toma mtx
    FILE *f = fopen(csv_path, "w");
    if (!f) return -1;
    guardar_todos(f);
    fclose(f);
    return 0;
}
int guardar_arch(void) {                      // SÍ toma mtx
    int rc;
    pthread_mutex_lock(&mtx);
    rc = guardar_arch_locked();
    pthread_mutex_unlock(&mtx);
    return rc;
}

/* ====== API pública ====== */
int abrir_arch(const char *path) {
    FILE *f;
    char linea[512];

    pthread_mutex_lock(&mtx);
    strncpy(csv_path, path, sizeof(csv_path)-1);

    free(productos);
    productos = NULL;
    productos_tam = productos_cap = 0;

    f = fopen(path, "r");
    if (!f) { pthread_mutex_unlock(&mtx); return -1; }

 if (!fgets(linea, sizeof(linea), f)) {
        fclose(f);
        pthread_mutex_unlock(&mtx);
        return -1; // archivo vacío o error
    }
    while (fgets(linea, sizeof(linea), f)) {
        registro_t r = {0};
        if (!parsear_linea(linea, &r)) {
            asegurar_capacidad();
            productos[productos_tam++] = r;
        }
    }

    fclose(f);
    pthread_mutex_unlock(&mtx);
    return 0;
}

void cerrar_arch(void) {
    pthread_mutex_lock(&mtx);
    free(productos);
    productos = NULL;
    productos_tam = productos_cap = 0;
    pthread_mutex_unlock(&mtx);
}

int recargar_arch(void) {
    return abrir_arch(csv_path);
}

/* ===== CRUD ===== */
int buscar_id_arch(int id, registro_t *out) 
{
    pthread_mutex_lock(&mtx);
    for (size_t i = 0; i < productos_tam; i++) {
        if (productos[i].id == id && !productos[i].borrado) {
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
    if (!r) return -1;

    pthread_mutex_lock(&mtx);                 // único lock
    for (size_t i = 0; i < productos_tam; i++)
        if (productos[i].id > maxid) maxid = productos[i].id;

    registro_t nuevo = *r;
    if (!nuevo.id) nuevo.id = maxid + 1;

    asegurar_capacidad();
    productos[productos_tam++] = nuevo;

    rc = guardar_arch_locked();               // evita doble lock
    pthread_mutex_unlock(&mtx);
    return rc;
}

int actualizar_arch(const registro_t *patch) 
{
    int rc = -1;
    if (!patch) return -1;

    pthread_mutex_lock(&mtx);                 // único lock
    for (size_t i = 0; i < productos_tam; i++) {
        if (productos[i].id == patch->id) {
            if (patch->nombre[0]) {
                strncpy(productos[i].nombre, patch->nombre, NOMBRE_MAXLEN - 1);
                productos[i].nombre[NOMBRE_MAXLEN - 1] = '\0';
            }
            if (patch->precio > 0) productos[i].precio = patch->precio;
            productos[i].stock = patch->stock;

            rc = guardar_arch_locked();       // evita doble lock
            pthread_mutex_unlock(&mtx);
            return rc;
        }
    }
    pthread_mutex_unlock(&mtx);
    return -1;
}

int eliminar_arch(int id) {
    int rc = -1;

    pthread_mutex_lock(&mtx);                 // único lock
    for (size_t i = 0; i < productos_tam; i++) {
        if (productos[i].id == id && !productos[i].borrado) {
            productos[i].borrado = true;
            rc = guardar_arch_locked();       // evita doble lock
            pthread_mutex_unlock(&mtx);
            return rc;
        }
    }
    pthread_mutex_unlock(&mtx);
    return -1;
}

int generar_snapshot(void) 
{
    pthread_mutex_lock(&mtx);
    if (snapshot_on) 
    { 
        pthread_mutex_unlock(&mtx); 
        return -1; 
    }
    snapshot_cap = productos_cap;
    snapshot_tam = productos_tam;
    snapshot = (registro_t*)malloc(sizeof(registro_t) * (snapshot_cap ? snapshot_cap : 1));
    if (!snapshot) 
    { 
        pthread_mutex_unlock(&mtx); 
        return -1; 
    }
    if (productos_tam) memcpy(snapshot, productos, sizeof(registro_t) * productos_tam);
    snapshot_on = 1;
    pthread_mutex_unlock(&mtx);
    return 0;
}

int guardar_snapshot(void) 
{
    pthread_mutex_lock(&mtx);
    if (snapshot_on) 
    {
        free(snapshot);
        snapshot = NULL;
        snapshot_tam = snapshot_cap = 0;
        snapshot_on = 0;
    }
    pthread_mutex_unlock(&mtx);
    return 0;
}

int descartar_snapshot(void) 
{
    registro_t *tmp;

    pthread_mutex_lock(&mtx);
    if (!snapshot_on) 
    { 
        pthread_mutex_unlock(&mtx); 
        return -1; 
    }

    if (productos_cap < snapshot_cap) 
    {
        tmp = (registro_t*)realloc(productos, sizeof(registro_t) * snapshot_cap);
        if (!tmp) 
        {
            tmp = (registro_t*)realloc(productos, sizeof(registro_t) * snapshot_tam);
            if (!tmp) 
            { 
                pthread_mutex_unlock(&mtx); 
                return -2; 
            }
            productos_cap = snapshot_tam;
        } else {
            productos_cap = snapshot_cap;
        }
        productos = tmp;
    }

    if (snapshot_tam) memcpy(productos, snapshot, sizeof(registro_t) * snapshot_tam);
    productos_tam = snapshot_tam;

    free(snapshot);
    snapshot = NULL;
    snapshot_tam = snapshot_cap = 0;
    snapshot_on = 0;

    int rc = guardar_arch_locked();           // persiste lo restaurado
    pthread_mutex_unlock(&mtx);
    return rc;
}

/* ===== NUEVO: búsquedas por nombre ===== */

/* Primer match por subcadena (case-insensitive, no borrado) */
int buscar_nombre_primero(const char *buscado, registro_t *out) {
    int rc = -1;
    pthread_mutex_lock(&mtx);
    for (size_t i = 0; i < productos_tam; ++i) 
    {
        if (!productos[i].borrado && buscar_cadena(productos[i].nombre, buscado)) 
        {
            if (out) *out = productos[i];
            rc = 0;
            break;
        }
    }
    pthread_mutex_unlock(&mtx);
    return rc; // 0 si encontró, -1 si no
}

/* Todas las coincidencias; devuelve array (malloc) y cantidad */
int buscar_nombre_todos(const char *buscado, registro_t **outs, size_t *cont) {
    size_t c = 0;
    if (!outs || !cont) return -1;
    *outs = NULL; *cont = 0;

    pthread_mutex_lock(&mtx);
    // 1ª pasada: contar
    for (size_t i = 0; i < productos_tam; ++i)
        if (!productos[i].borrado && buscar_cadena(productos[i].nombre, buscado))
            ++c;

    if (!c) 
    { 
        pthread_mutex_unlock(&mtx); 
        return -1; 
    }

    // 2ª pasada: copiar
    registro_t *vec = (registro_t*)malloc(sizeof(registro_t) * c);
    if (!vec) { 
        pthread_mutex_unlock(&mtx); 
        return -1; 
    }

    size_t j = 0;
    for (size_t i = 0; i < productos_tam; ++i)
        if (!productos[i].borrado && buscar_cadena(productos[i].nombre, buscado))
            vec[j++] = productos[i];

    pthread_mutex_unlock(&mtx);

    *outs = vec;
    *cont = c;
    return 0;
}
