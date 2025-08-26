#include "csv.h"
int csv_append(FILE* f, const record_t* r){
    return fprintf(f, "%llu,%s,%s,%.6f\n",
      (unsigned long long)r->id, r->f1, r->f2, r->f3) > 0 ? 0 : -1;
}
FILE* csv_open(const char* path, int with_header){
    FILE* f = fopen(path, "w");
    if (!f) return NULL;
    if (with_header) fprintf(f, "id,f1,f2,f3\n");
    return f;
}
void csv_close(FILE* f){ if (f) fclose(f); }
