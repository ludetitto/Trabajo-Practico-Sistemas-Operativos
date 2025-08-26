#ifndef CSV_H
#define CSV_H
#include "common.h"

FILE* csv_open(const char* path, int write_header);
void  csv_write(FILE* f, const record_t* r);
void  csv_close(FILE* f);

#endif
