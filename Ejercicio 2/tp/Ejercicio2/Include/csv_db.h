#ifndef CSV_DB_H
#define CSV_DB_H

#include <sys/types.h>
#include <stddef.h>

#define MAX_NAME 64
#define MAX_TS   32

typedef struct {
    int id;
    char name[MAX_NAME];
    double price;
    int stock;
    int generator_pid;
    char ts[MAX_TS];
} Product;

/* Cargar la DB (desde CSV) en memoria */
int db_load(const char *path);
/* Escribir la DB en disco de forma atómica (tmp -> rename) */
int db_write_atomic(const char *path);
/* Liberar memoria */
void db_free(void);

/* Operaciones */
ssize_t db_find_index_by_id(int id);
int db_get_by_id(int id, Product *out);
int db_list_all(char *buf, size_t bufsz, size_t *used);
int db_insert_product(const char *name, double price, int stock, int *out_id);
int db_update_field(int id, const char *field, const char *value);
int db_delete(int id);
int db_max_id(void);

#endif





