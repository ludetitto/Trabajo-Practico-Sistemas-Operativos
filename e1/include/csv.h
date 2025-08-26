#pragma once
#include "common.h"
#include <stdio.h>
FILE* csv_open(const char* path, int with_header);
int   csv_append(FILE* f, const record_t* r);
void  csv_close(FILE* f);
