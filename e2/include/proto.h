#ifndef PROTO_H
#define PROTO_H
#include "common.h"
#include "csvdb.h"

// helpers para parsear "k=v" (nombre, generador, pid)
int parse_kv_int(const char* token, const char* key, int* out);
int parse_kv_str(const char* token, const char* key, char* out, size_t max);

#endif
