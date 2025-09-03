#include "../include/csv.h"
#include "../include/cola.h"

FILE *abrir_csv(const char *path, int incluir_encabezado)
{
    FILE *f = fopen(path, "w");
    if (!f)
        return NULL;
    if (incluir_encabezado)
    {
        fprintf(f, "id,generador,pid,Nombre\n");
        fflush(f);
    }
    return f;
}

void escribir_csv(FILE *f, const registro_t *reg)
{
    fprintf(f, "%u,%d,%d,%s\n", reg->id, reg->generador, (int)reg->pid, reg->nombre);
    cola_sidecar_log(reg); // ya compila porque ahora está el prototipo
}

void cerrar_csv(FILE *f)
{
    if (f)
        fclose(f);
}