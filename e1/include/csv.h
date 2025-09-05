#ifndef CSV_H
#define CSV_H
#include "common.h"

FILE* abrir_csv(const char* path, int write_header);
void  escribir_csv(FILE* f, const registro_t* r);
void  cerrar_csv(FILE* f);

#endif
