#ifndef CSVDB_H
#define CSVDB_H
#include "common.h"

typedef struct {
  int id;
  int generador;
  int pid;
  char nombre[NAME_MAXLEN];
} row_t;

typedef struct {
  row_t *v;
  size_t len, cap;
} db_t;

int   db_load(const char* path, db_t* db);
void  db_free(db_t* db);
int   db_write_atomic(const char* path, const db_t* db); // tmp + rename

int   db_max_id(const db_t* db);
int   db_find_by_id(const db_t* db, int id, row_t* out);
int   db_find_by_nombre(const db_t* db, const char* nombre, row_t** out_list, size_t* out_n); // malloc

// DML sobre snapshot en memoria
int   db_add(db_t* db, const row_t* r);                 // id debe venir seteado
int   db_update(db_t* db, int id, const row_t* patch);  // patch: sólo campos >-1 o nombre no vacío
int   db_delete(db_t* db, int id);

#endif
