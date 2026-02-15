#define _POSIX_C_SOURCE 200809L
#include "csv_db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <strings.h>


static Product *table = NULL;
static size_t table_len = 0;
static size_t table_cap = 0;
static int current_max_id = 0;

static void ensure_capacity(void) {
    if (table_len < table_cap) return;
    size_t nc = table_cap ? table_cap * 2 : 128;
    Product *p = realloc(table, nc * sizeof(Product));
    if (!p) {
        perror("realloc db");
        exit(EXIT_FAILURE);
    }
    table = p;
    table_cap = nc;
}

int db_load(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        return -1; /* ok: caller can continue with empty DB */
    }
    char line[512];
    /* try to skip header if present */
    if (!fgets(line, sizeof(line), f)) { fclose(f); return 0; }
    if (!strchr(line, ',')) rewind(f);

    table_len = 0;
    current_max_id = 0;

    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '\n' || line[0] == '\0') continue;
        Product prod;
        prod.name[0] = '\0'; prod.ts[0] = '\0';
        int scanned = sscanf(line, "%d,%63[^,],%lf,%d,%d,%31[^\n]",
               &prod.id, prod.name, &prod.price, &prod.stock, &prod.generator_pid, prod.ts);
        if (scanned >= 3) {
            ensure_capacity();
            table[table_len++] = prod;
            if (prod.id > current_max_id) current_max_id = prod.id;
        }
    }
    fclose(f);
    return 0;
}

int db_write_atomic(const char *path) {
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    if (!f) {
        perror("db_write fopen tmp");
        return -1;
    }
    fprintf(f, "ID,NombreProducto,Precio,Stock,GeneradorPID,Timestamp\n");
    for (size_t i = 0; i < table_len; ++i) {
        Product *pr = &table[i];
        fprintf(f, "%d,%s,%.2f,%d,%d,%s\n",
                pr->id, pr->name, pr->price, pr->stock, pr->generator_pid, pr->ts);
    }
    fflush(f);
    if (fclose(f) != 0) {
        perror("fclose tmp");
        unlink(tmp);
        return -1;
    }
    if (rename(tmp, path) != 0) {
        perror("rename tmp->path");
        unlink(tmp);
        return -1;
    }
    return 0;
}

void db_free(void) {
    free(table);
    table = NULL;
    table_len = table_cap = 0;
    current_max_id = 0;
}

/* helpers */
ssize_t db_find_index_by_id(int id) {
    for (size_t i = 0; i < table_len; ++i)
        if (table[i].id == id) return (ssize_t)i;
    return -1;
}

int db_get_by_id(int id, Product *out) {
    ssize_t idx = db_find_index_by_id(id);
    if (idx < 0) return -1;
    if (out) *out = table[idx];
    return 0;
}

int db_list_all(char *buf, size_t bufsz, size_t *used) {
    size_t pos = 0;
    for (size_t i = 0; i < table_len; ++i) {
        int n = snprintf(buf + pos, bufsz - pos, "%d,%s,%.2f,%d,%d,%s\n",
                         table[i].id, table[i].name, table[i].price,
                         table[i].stock, table[i].generator_pid, table[i].ts);
        if (n < 0 || (size_t)n >= bufsz - pos) return -1;
        pos += (size_t)n;
    }
    if (used) *used = pos;
    return 0;
}

int db_insert_product(const char *name, double price, int stock, int *out_id) {
    ensure_capacity();
    int nid = ++current_max_id;
    Product p;
    p.id = nid;
    strncpy(p.name, name, MAX_NAME-1);
    p.name[MAX_NAME-1] = '\0';
    p.price = price;
    p.stock = stock;
    p.generator_pid = 0;
    p.ts[0] = '\0';
    table[table_len++] = p;
    if (out_id) *out_id = nid;
    return 0;
}

int db_update_field(int id, const char *field, const char *value) {
    ssize_t idx = db_find_index_by_id(id);
    if (idx < 0) return -1;
    Product *p = &table[idx];
    if (strcasecmp(field, "name") == 0) {
        strncpy(p->name, value, MAX_NAME-1);
        p->name[MAX_NAME-1] = '\0';
    } else if (strcasecmp(field, "price") == 0) {
        p->price = atof(value);
    } else if (strcasecmp(field, "stock") == 0) {
        p->stock = atoi(value);
    } else {
        return -2; /* unknown field */
    }
    return 0;
}

int db_delete(int id) {
    ssize_t idx = db_find_index_by_id(id);
    if (idx < 0) return -1;
    table[idx] = table[table_len - 1];
    table_len--;
    return 0;
}

int db_max_id(void) { return current_max_id; }


