#include "../include/csv.h"

FILE *abrir_csv(const char *path, int incluir_encabezado)
{
    FILE *f = fopen(path, "w");
    if (!f)
        return NULL;

    if (incluir_encabezado)
    {
        // Nuevo esquema
        // Usamos punto decimal estándar; si tu locale usa coma, igual forzamos '.' en la salida.
        fprintf(f, "id,generador,pid,producto,precio,stock\n");
        fflush(f);
    }
    return f;
}

void escribir_csv(FILE *f, const registro_t *reg)
{
    // nombre = producto (se mantiene el campo para no romper otros módulos).
    // Precio con 2 decimales.
    fprintf(f, "%u,%d,%d,%s,%.2f,%u\n",
            reg->id,
            reg->generador,
            (int)reg->pid,
            reg->nombre,
            (double)reg->precio,
            reg->stock);
    fflush(f);
}

void cerrar_csv(FILE *f)
{
    if (f)
        fclose(f);
}
