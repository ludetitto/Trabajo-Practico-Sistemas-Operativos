// proto.c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include "../include/proto.h"
#include "../include/csvdb.h"

/* =========================
   Helpers de copia/concat
   ========================= */
static void safe_copy(char *dst, size_t dstsz, const char *src) {
    if (!dst || dstsz == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t n = strlen(src);
    if (n >= dstsz) n = dstsz - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static void safe_cat_raw(char *dst, size_t *used_io, size_t cap, const char *src, size_t src_len) {
    if (!dst || !used_io || !src || cap == 0) return;
    size_t used = *used_io;
    if (used >= cap - 1) return;
    size_t avail  = cap - 1 - used;
    size_t tocopy = (src_len < avail) ? src_len : avail;
    if (tocopy > 0) {
        memcpy(dst + used, src, tocopy);
        used += tocopy;
        dst[used] = '\0';
    }
    *used_io = used;
}

static void safe_cat_cstr(char *dst, size_t *used_io, size_t cap, const char *src) {
    safe_cat_raw(dst, used_io, cap, src, strlen(src));
}

/* =========================
   Helpers de parsing
   ========================= */
static void rstrip(char *s) {
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n-1])) s[--n] = 0;
}
static void lskip(const char **ps) {
    const char *p = *ps;
    while (*p && isspace((unsigned char)*p)) p++;
    *ps = p;
}
static void upper(char *s) {
    for (; *s; s++) *s = (char)toupper((unsigned char)*s);
}

/* =========================
   parseo de ADD (CSV o k=v)
   ========================= */
static int parse_add_any(const char *args, char *nombre_out, float *precio_out, int *stock_out) {
    /* ---- 1) CSV: Nombre,precio,stock ---- */
    {
        char nom_tmp[NOMBRE_MAXLEN]; float pr = 0.0f; int st = 0;
        if (sscanf(args, " %63[^,] , %f , %d ", nom_tmp, &pr, &st) == 3) {
            /* trim del nombre */
            size_t L = strlen(nom_tmp);
            while (L && isspace((unsigned char)nom_tmp[L-1])) nom_tmp[--L] = 0;
            while (*nom_tmp && isspace((unsigned char)*nom_tmp)) memmove(nom_tmp, nom_tmp+1, --L);
            safe_copy(nombre_out, NOMBRE_MAXLEN, nom_tmp);
            *precio_out = pr;
            *stock_out  = st;
            return 0;
        }
    }

    /* ---- 2) k=v: nombre=... precio=... stock=... (orden libre) ---- */
    {
        char nombre[NOMBRE_MAXLEN] = {0};
        float precio = -1.0f;
        int stock = -1;

        const char *p = args;
        while (*p) {
            while (*p && isspace((unsigned char)*p)) p++;
            if (!*p) break;

            /* token hasta espacio */
            const char *start = p;
            while (*p && !isspace((unsigned char)*p)) p++;
            size_t len = (size_t)(p - start);
            if (len == 0) continue;

            char tok[256];
            if (len >= sizeof(tok)) len = sizeof(tok) - 1;
            memcpy(tok, start, len); tok[len] = '\0';

            char *eq = strchr(tok, '=');
            if (!eq) {
                /* token suelto sin '=', solo se usa como parte del nombre si ya venimos armando */
                continue;
            }

            *eq = '\0';
            char *k = tok;
            char *v = eq + 1;
            for (char *q = k; *q; ++q) *q = (char)tolower((unsigned char)*q);

            if (!strcmp(k, "precio")) {
                precio = (float)atof(v);
            } else if (!strcmp(k, "stock")) {
                stock = atoi(v);
            } else if (!strcmp(k, "nombre") || !strcmp(k, "producto")) {
                /* nombre puede tener espacios y/o venir entre comillas */
                size_t used = 0;
                nombre[0] = '\0';

                /* caso comillado: nombre="Joystick PS5"   */
                if (v[0] == '"') {
                    /* quitar primera comilla */
                    v++; 
                    /* copiar lo que sigue en este token hasta fin (puede no cerrar aún) */
                    safe_copy(nombre, NOMBRE_MAXLEN, v);
                    used = strlen(nombre);

                    /* si el token actual ya cerraba con comilla */
                    size_t L = strlen(nombre);
                    if (L > 0 && nombre[L-1] == '"') {
                        nombre[L-1] = '\0'; /* quitar comilla final */
                        used = strlen(nombre);
                    } else {
                        /* seguir consumiendo tokens hasta encontrar " de cierre */
                        const char *look = p;
                        int cerrado = 0;
                        while (*look && !cerrado) {
                            while (*look && isspace((unsigned char)*look)) look++;
                            if (!*look) break;

                            const char *ls = look;
                            while (*look && !isspace((unsigned char)*look)) look++;
                            size_t llen = (size_t)(look - ls);
                            if (llen == 0) break;

                            char nxt[256];
                            if (llen >= sizeof(nxt)) llen = sizeof(nxt) - 1;
                            memcpy(nxt, ls, llen); nxt[llen] = '\0';

                            /* ¿cierra con comilla? */
                            size_t NL = strlen(nxt);
                            if (NL > 0 && nxt[NL-1] == '"') {
                                nxt[NL-1] = '\0'; /* quitar comilla final */
                                if (used < NOMBRE_MAXLEN - 1) safe_cat_cstr(nombre, &used, NOMBRE_MAXLEN, " ");
                                safe_cat_cstr(nombre, &used, NOMBRE_MAXLEN, nxt);
                                cerrado = 1;
                                p = look; /* avanzar p porque consumimos este token */
                                break;
                            } else {
                                if (used < NOMBRE_MAXLEN - 1) safe_cat_cstr(nombre, &used, NOMBRE_MAXLEN, " ");
                                safe_cat_cstr(nombre, &used, NOMBRE_MAXLEN, nxt);
                                p = look; /* consumimos el token como parte del nombre */
                            }
                        }
                        /* si no encontramos cierre, igual seguimos con lo recabado */
                    }
                } else {
                    /* sin comillas: nombre=Joystick PS5 ...  -> glue hasta ver otra clave con '=' */
                    safe_copy(nombre, NOMBRE_MAXLEN, v);
                    used = strlen(nombre);

                    const char *look = p;
                    while (*look) {
                        while (*look && isspace((unsigned char)*look)) look++;
                        if (!*look) break;

                        const char *ls = look;
                        while (*look && !isspace((unsigned char)*look)) look++;
                        size_t llen = (size_t)(look - ls);
                        if (llen == 0) break;

                        char nxt[256];
                        if (llen >= sizeof(nxt)) llen = sizeof(nxt) - 1;
                        memcpy(nxt, ls, llen); nxt[llen] = '\0';

                        if (strchr(nxt, '=')) {
                            /* otra clave -> dejamos que el while exterior la procese */
                            break;
                        } else {
                            if (used < NOMBRE_MAXLEN - 1) safe_cat_cstr(nombre, &used, NOMBRE_MAXLEN, " ");
                            safe_cat_cstr(nombre, &used, NOMBRE_MAXLEN, nxt);
                            p = look; /* consumimos el token como parte del nombre */
                        }
                    }
                }
            }
        }

        if (nombre[0] && precio >= 0.0f && stock >= 0) {
            safe_copy(nombre_out, NOMBRE_MAXLEN, nombre);
            *precio_out = precio;
            *stock_out  = stock;
            return 0;
        }
    }

    return -1; /* no matcheó */
}

/* =========================
   Intérprete del protocolo
   ========================= */
void procesar_linea_protocolo(int cfd, const char *linea)
{
    char buf[1024], cmd[32] = {0};
    const char *pbuf;
    int i = 0, local_tx_active, local_tx_owner;
    registro_t *vec = NULL, r;
    size_t n = 0;

    safe_copy(buf, sizeof(buf), linea);
    rstrip(buf);
    pbuf = buf; lskip(&pbuf);
    if (!*pbuf) { dprintf(cfd, "ERR EMPTY\n"); return; }

    while (pbuf[i] && !isspace((unsigned char)pbuf[i]) && i < (int)sizeof(cmd) - 1) { cmd[i] = pbuf[i]; i++; }
    cmd[i] = 0; upper(cmd); pbuf += i; lskip(&pbuf);

    /* snapshot de TX (quién es el dueño, etc.) */
    pthread_mutex_lock(&tx_mtx);
    local_tx_active = tx_active;
    local_tx_owner  = tx_owner;
    pthread_mutex_unlock(&tx_mtx);

    /* ----------- PING ----------- */
    if (!strcmp(cmd, "PING")) { dprintf(cfd, "OK\n"); return; }

    /* ----------- GET <id> ----------- */
    if (!strcmp(cmd, "GET")) {
        int id = atoi(pbuf);
        if (local_tx_active && local_tx_owner != cfd) { dprintf(cfd, "ERR TX_ACTIVE\n"); return; }
        if (!buscar_id_arch(id, &r))
            dprintf(cfd, "RESULT %d,%s,%.2f,%u,%s\n", r.id, r.nombre, r.precio, r.stock, r.timestamp);
        else
            dprintf(cfd, "ERR NOT_FOUND\n");
        return;
    }

    /* ----------- FIND / FIND ALL ----------- */
    if (!strcmp(cmd, "FIND")) {
        if (!strncasecmp(pbuf, "ALL", 3) && isspace((unsigned char)pbuf[3])) {
            pbuf += 3; while (*pbuf && isspace((unsigned char)*pbuf)) pbuf++;
            if (local_tx_active && local_tx_owner != cfd) { dprintf(cfd, "ERR TX_ACTIVE\n"); return; }
            if (buscar_nombre_todos(pbuf, &vec, &n) != 0) { dprintf(cfd, "END\n"); return; }
            for (size_t k = 0; k < n; ++k)
                dprintf(cfd, "ROW %d,%s,%.2f,%u,%s\n", vec[k].id, vec[k].nombre, vec[k].precio, vec[k].stock, vec[k].timestamp);
            free(vec);
            dprintf(cfd, "END\n");
            return;
        }
        if (local_tx_active && local_tx_owner != cfd) { dprintf(cfd, "ERR TX_ACTIVE\n"); return; }
        if (buscar_nombre_primero(pbuf, &r) == 0)
            dprintf(cfd, "RESULT %d,%s,%.2f,%u,%s\n", r.id, r.nombre, r.precio, r.stock, r.timestamp);
        else
            dprintf(cfd, "ERR NOT_FOUND\n");
        return;
    }

    /* ====== DML: requieren TX activa y ser dueño ====== */

    /* ----------- ADD ----------- */
    if (!strcmp(cmd, "ADD")) {
        if (!local_tx_active)      { dprintf(cfd, "ERR NOT_TX_ACTIVE\n"); return; }
        if (local_tx_owner != cfd) { dprintf(cfd, "ERR TX_ACTIVE\n");    return; }

        char nombre[NOMBRE_MAXLEN] = {0};
        float precio = -1.0f;
        int stock = -1;

        if (parse_add_any(pbuf, nombre, &precio, &stock) != 0) {
            dprintf(cfd, "ERR ARG\n"); return;
        }

        registro_t nr = {0};
        safe_copy(nr.nombre, NOMBRE_MAXLEN, nombre);
        nr.precio = precio;
        nr.stock  = stock;
        time_t ahora = time(NULL);
        strftime(nr.timestamp, sizeof(nr.timestamp), "%Y-%m-%d %H:%M:%S", localtime(&ahora));
        nr.borrado = false;

        if (!agregar_arch(&nr)) dprintf(cfd, "OK\n");
        else                    dprintf(cfd, "ERR IO\n");
        return;
    }

    /* ----------- UPDATE id,nombre,precio,stock ----------- */
    if (!strcmp(cmd, "UPDATE")) {
        if (!local_tx_active)      { dprintf(cfd, "ERR NOT_TX_ACTIVE\n"); return; }
        if (local_tx_owner != cfd) { dprintf(cfd, "ERR TX_ACTIVE\n");    return; }

        int id; char nombre[NOMBRE_MAXLEN]; float precio; int stock;
        registro_t patch = {0};
        if (sscanf(pbuf, "%d,%63[^,],%f,%d", &id, nombre, &precio, &stock) < 4) {
            dprintf(cfd, "ERR ARG\n"); return;
        }

        patch.id = id;
        safe_copy(patch.nombre, NOMBRE_MAXLEN, nombre);
        patch.precio = precio;
        patch.stock  = stock;

        if (!actualizar_arch(&patch)) dprintf(cfd, "OK\n");
        else                          dprintf(cfd, "ERR NOT_FOUND\n");
        return;
    }

    /* ----------- DELETE <id> ----------- */
    if (!strcmp(cmd, "DELETE")) {
        if (!local_tx_active)      { dprintf(cfd, "ERR NOT_TX_ACTIVE\n"); return; }
        if (local_tx_owner != cfd) { dprintf(cfd, "ERR TX_ACTIVE\n");    return; }

        int id = atoi(pbuf);
        if (id <= 0) { dprintf(cfd, "ERR ARG\n"); return; }

        if (!eliminar_arch(id)) dprintf(cfd, "OK\n");
        else                    dprintf(cfd, "ERR NOT_FOUND\n");
        return;
    }

    /* ----------- QUIT ----------- */
    if (!strcmp(cmd, "QUIT")) { dprintf(cfd, "BYE\n"); return; }

    dprintf(cfd, "ERR CMD\n");
}
