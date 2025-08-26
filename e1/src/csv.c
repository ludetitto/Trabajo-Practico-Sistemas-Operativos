#include "../include/csv.h"

FILE* csv_open(const char* path, int write_header) {
    FILE* f = fopen(path, "w");
    if (!f) return NULL;
    if (write_header) {
        fprintf(f, "id,generador,pid,Nombre\n");
        fflush(f);
    }
    return f;
}

void csv_write(FILE* f, const record_t* r) {
    fprintf(f, "%u,%d,%d,%s\n", r->id, r->generador, (int)r->pid, r->nombre);
}

void csv_close(FILE* f) {
    if (f) fclose(f);
}
