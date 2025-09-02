#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "../include/common.h"
#include "../include/cola.h"
#include "../include/eventos.h"

/* =====================  Infra básica existente (se respeta) ===================== */

static FILE *g_csv_cola = NULL;

/* Cola global (solo traza, sin impacto en posiciones por evento) */
typedef struct
{
    uint32_t *v;
    size_t cap, head, tail, len;
} cola_ids_t;

static cola_ids_t g_cola = {0};

static void cola_init(cola_ids_t *q)
{
    q->v = NULL;
    q->cap = q->head = q->tail = q->len = 0;
}

static void cola_push(cola_ids_t *q, uint32_t id)
{
    if (q->len + 1 > q->cap)
    {
        size_t nc = q->cap ? q->cap * 2 : 64;
        uint32_t *nv = (uint32_t *)realloc(q->v, nc * sizeof(uint32_t));
        if (!nv)
            return;
        if (q->len && q->head)
        {
            for (size_t i = 0; i < q->len; i++)
                nv[i] = q->v[(q->head + i) % q->cap];
            q->head = 0;
            q->tail = q->len;
        }
        q->v = nv;
        q->cap = nc;
    }
    q->v[q->tail] = id;
    q->tail = (q->tail + 1) % q->cap;
    q->len++;
}

static int archivo_vacio(FILE *f)
{
    long p = ftell(f);
    if (p < 0)
        return 0;
    if (fseek(f, 0, SEEK_END) != 0)
        return 0;
    long sz = ftell(f);
    if (sz < 0)
        sz = 0;
    (void)fseek(f, p, SEEK_SET);
    return sz == 0;
}

static void abrir_cola_csv_unico(void)
{
    if (g_csv_cola)
        return;
    g_csv_cola = fopen("cola.csv", "a+");
    if (!g_csv_cola)
    {
        perror("cola.csv");
        return;
    }
    if (archivo_vacio(g_csv_cola))
    {
        /* ÚNICO CSV con posiciones por evento */
        fprintf(g_csv_cola, "id,posicion,estado,evento,nombre\n");
        fflush(g_csv_cola);
    }
}

/* =====================  Posición POR EVENTO en un único CSV ==================== */

/* Importante: mantener en sync con eventos.h/.c */
static const char *EVTS[] = {
    "Lollapalooza",
    "Cosquin Rock",
    "Bresh"};
#define NEVENTOS (sizeof(EVTS) / sizeof(EVTS[0]))

/* contador de posición por evento */
static unsigned pos_evt[NEVENTOS]; /* se zero-inicializa estáticamente */

/* devuelve el índice del evento o -1 si no está */
static int idx_evento(const char *e)
{
    for (size_t i = 0; i < NEVENTOS; ++i)
        if (strcmp(EVTS[i], e) == 0)
            return (int)i;
    return -1;
}

/* =====================  Ciclo de vida del sidecar =============================== */

__attribute__((constructor)) static void init_sidecar(void)
{
    cola_init(&g_cola);
    abrir_cola_csv_unico();
    for (size_t i = 0; i < NEVENTOS; i++)
        pos_evt[i] = 0; /* <-- acá estaba el g_evt fantasma */
}

__attribute__((destructor)) static void fini_sidecar(void)
{
    if (g_csv_cola)
        fclose(g_csv_cola);
    free(g_cola.v);
}

/* =====================  Punto de integración con el CSV principal =============== */

void cola_sidecar_log(const registro_t *r)
{
    if (!r)
        return;             /* defensa */
    abrir_cola_csv_unico(); /* lazy-open único CSV */

    /* mantenemos una traza global (opcional) */
    cola_push(&g_cola, r->id);

    /* Elegimos evento (hoy al azar; podría venir en r en el futuro) */
    const char *evento = evento_aleatorio();
    int ie = idx_evento(evento);
    unsigned posicion = (ie >= 0) ? ++pos_evt[ie] : 0; /* posición POR EVENTO */

    /* nombre: r->nombre es un arreglo, nunca NULL */
    const char *nombre = (r->nombre[0]) ? r->nombre : "Anonimo"; /* <-- faltaba esta línea */

    fprintf(g_csv_cola, "%u,%u,%s,%s,%s\n",
            r->id, posicion, "Esperando", evento, nombre);
    fflush(g_csv_cola);
}
