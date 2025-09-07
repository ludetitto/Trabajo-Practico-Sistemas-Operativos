#ifndef PROTO_H
#define PROTO_H
#include "common.h"
#include "csvdb.h"

// helpers para parsear "k=v" (nombre, generador, pid)
int parsear_kv_entero(const char *token, const char *llave, int *out);
int parsear_kv_str(const char *token, const char *llave, char *out, size_t max);

#endif
