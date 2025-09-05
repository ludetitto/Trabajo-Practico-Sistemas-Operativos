#ifndef CSV_H
#define CSV_H
#include "common.h"

// Abre el CSV en modo escritura ("w"). Si incluir_encabezado != 0, escribe la cabecera.
FILE *abrir_csv(const char *path, int incluir_encabezado);

// Escribe una fila usando los campos de registro_t (nombre = producto).
void escribir_csv(FILE *f, const registro_t *r);

// Cierra el archivo si no es NULL.
void cerrar_csv(FILE *f);

#endif
