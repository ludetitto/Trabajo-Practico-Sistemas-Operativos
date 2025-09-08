#ifndef PROTO_H
#define PROTO_H

#include "common.h"
#include "csvdb.h"

/* Helpers para parsear parámetros tipo "k=v" */
int parsear_kv_entero(const char *token, const char *llave, int *out);
int parsear_kv_str(const char *token, const char *llave, char *out, size_t max);
int parsear_kv_float(const char *token, const char *llave, float *out);

#endif
