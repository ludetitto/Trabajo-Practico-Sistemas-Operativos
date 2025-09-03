#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "../include/common.h"
#include "../include/cola.h"
#include "../include/eventos.h"

/* =====================  Infra básica existente (se respeta) ===================== */

static FILE *cola_csv = NULL;

/* Cola global (solo traza, sin impacto en posiciones por evento) */
typedef struct
{
    uint32_t *buf;
    size_t capacidad, pri, ult, tam;
} cola_ids_t;

static cola_ids_t cola_ids = {0};

static void crear_cola(cola_ids_t *cola)
{
    cola->buf = NULL;
    cola->capacidad = cola->pri = cola->ult = cola->tam = 0;
}

static void poner_en_cola(cola_ids_t *cola, uint32_t id)
{
    if (cola->tam + 1 > cola->capacidad)
    {
        size_t nueva_capacidad = cola->capacidad ? cola->capacidad * 2 : 64;
        uint32_t *nuevo_buf = (uint32_t *)realloc(cola->buf, nueva_capacidad * sizeof(uint32_t));
        if (!nuevo_buf)
            return;
        if (cola->tam && cola->pri)
        {
            for (size_t i = 0; i < cola->tam; i++)
                nuevo_buf[i] = cola->buf[(cola->pri + i) % cola->capacidad];
            cola->pri = 0;
            cola->ult = cola->tam;
        }
        cola->buf = nuevo_buf;
        cola->capacidad = nueva_capacidad;
    }
    cola->buf[cola->ult] = id;
    cola->ult = (cola->ult + 1) % cola->capacidad;
    cola->tam++;
}

static int archivo_vacio(FILE *f)
{
    long pos = ftell(f), tam;
    if (pos < 0)
        return 0;
    if (fseek(f, 0, SEEK_END) != 0)
        return 0;
    tam = ftell(f);
    if (tam < 0)
        tam = 0;
    (void)fseek(f, pos, SEEK_SET);
    return tam == 0;
}

static void abrir_cola_csv_unico(void)
{
    if (cola_csv)
        return;
    cola_csv = fopen("cola.csv", "a+");
    if (!cola_csv)
    {
        perror("cola.csv");
        return;
    }
    if (archivo_vacio(cola_csv))
    {
        /* ÚNICO CSV con posiciones por evento */
        fprintf(cola_csv, "id,posicion,estado,evento,nombre\n");
        fflush(cola_csv);
    }
}

/* =====================  Posición POR EVENTO en un único CSV ==================== */

/* Importante: mantener en sync con eventos.h/.c */
static const char *EVENTOS[] = {
    "Lollapalooza",
    "Cosquin Rock",
    "Bresh"};
#define NEVENTOS (sizeof(EVENTOS) / sizeof(EVENTOS[0]))

/* contador de posición por evento */
static unsigned pos_evt[NEVENTOS]; /* se zero-inicializa estáticamente */

/* devuelve el índice del evento o -1 si no está */
static int asignar_idx_evento(const char *e)
{
    for (size_t i = 0; i < NEVENTOS; ++i)
        if (strcmp(EVENTOS[i], e) == 0)
            return (int)i;
    return -1;
}

/* =====================  Ciclo de vida del sidecar =============================== */

__attribute__((constructor)) static void init_sidecar(void)
{
    crear_cola(&cola_ids);
    abrir_cola_csv_unico();
    for (size_t i = 0; i < NEVENTOS; i++)
        pos_evt[i] = 0; /* <-- acá estaba el g_evt fantasma */
}

__attribute__((destructor)) static void fini_sidecar(void)
{
    if (cola_csv)
        fclose(cola_csv);
    free(cola_ids.buf);
}

/* =====================  Punto de integración con el CSV principal =============== */

void cola_sidecar_log(const registro_t *reg)
{
    const char *evento, *nombre;
    int idx_evento;
    unsigned posicion;

    if (!reg)
        return;             /* defensa */
    abrir_cola_csv_unico(); /* lazy-open único CSV */

    /* mantenemos una traza global (opcional) */
    poner_en_cola(&cola_ids, reg->id);

    /* Elegimos evento (hoy al azar; podría venir en r en el futuro) */
    evento = evento_aleatorio();
    idx_evento = asignar_idx_evento(evento);
    posicion = (idx_evento >= 0) ? ++pos_evt[idx_evento] : 0; /* posición POR EVENTO */

    /* nombre: r->nombre es un arreglo, nunca NULL */
    nombre = (reg->nombre[0]) ? reg->nombre : "Anonimo"; /* <-- faltaba esta línea */

    fprintf(cola_csv, "%u,%u,%s,%s,%s\n",
            reg->id, posicion, "Esperando", evento, nombre);
    fflush(cola_csv);
}
