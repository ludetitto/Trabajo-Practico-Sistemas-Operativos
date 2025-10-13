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

/* trim espacios y comillas dobles alrededor */
static void trim_and_unquote(char *s) {
    if (!s) return;
    /* trim leading */
    char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    /* trim trailing */
    size_t L = strlen(s);
    while (L && isspace((unsigned char)s[L-1])) s[--L] = '\0';
    /* sacar comillas dobles si hay */
    if (L >= 2 && s[0] == '"' && s[L-1] == '"') {
        memmove(s, s+1, L-2);
        s[L-2] = '\0';
    }
}

/* =========================
   parseo CSV: Nombre,precio,stock
   ========================= */
static int parse_add_csv(const char *args, char *nombre_out, float *precio_out, int *stock_out) {
    char nom_tmp[NOMBRE_MAXLEN];
    float pr = 0.0f;
    int st = 0;

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
    return -1;
}

/* =========================
   parseo k=v: nombre=... precio=... stock=...
   – orden libre
   – nombre con o sin comillas
   ========================= */
/* =========================
   parseo k=v: nombre=... precio=... stock=...
   – orden libre
   – nombre con o sin comillas
   – SIN comillas: junta palabras siguientes del nombre hasta encontrar otra clave con '='
   ========================= */
static int parse_add_kv(const char *args, char *nombre_out, float *precio_out, int *stock_out) {
    const char *s = args;
    char nombre[NOMBRE_MAXLEN] = {0};
    float precio = -1.0f;
    int stock = -1;

    while (*s) {
        /* saltar espacios */
        while (*s && isspace((unsigned char)*s)) s++;
        if (!*s) break;

        /* leer clave hasta '=' o espacio */
        const char *kstart = s;
        while (*s && !isspace((unsigned char)*s) && *s != '=') s++;
        const char *kend = s;

        /* permitir "k = v" con espacios */
        while (*s && isspace((unsigned char)*s)) s++;
        if (*s != '=') {
            /* no es k=...; saltar este token suelto */
            while (*s && !isspace((unsigned char)*s)) s++;
            continue;
        }
        s++; /* '=' */

        /* normalizar clave a minúsculas y recortar caracteres no alfanuméricos
           Esto hace al parser más robust ante bytes extraños (p. ej. BOMs o
           caracteres invisibles) entre la clave y el '=' */
        char kbuf[32];
        size_t klen = (size_t)(kend - kstart);
        if (klen >= sizeof(kbuf)) klen = sizeof(kbuf) - 1;
        memcpy(kbuf, kstart, klen);
        kbuf[klen] = '\0';
        /* trim no alfanuméricos del inicio y fin */
        size_t kstart_off = 0, kend_off = klen;
        while (kstart_off < kend_off && !isalnum((unsigned char)kbuf[kstart_off])) kstart_off++;
        while (kend_off > kstart_off && !isalnum((unsigned char)kbuf[kend_off-1])) kend_off--;
        if (kstart_off > 0 || kend_off < klen) {
            size_t newlen = kend_off - kstart_off;
            if (newlen >= sizeof(kbuf)) newlen = sizeof(kbuf) - 1;
            memmove(kbuf, kbuf + kstart_off, newlen);
            kbuf[newlen] = '\0';
        }
        for (char *q = kbuf; *q; ++q) *q = (char)tolower((unsigned char)*q);

        /* saltar espacios previos al valor */
        while (*s && isspace((unsigned char)*s)) s++;

        /* leer valor base (comillado o simple) */
        char vbuf[256]; size_t vused = 0;
        int valor_comillado = 0;
        if (*s == '"') {
            valor_comillado = 1;
            s++; /* abrir comillas */
            while (*s && *s != '"' && vused < sizeof(vbuf)-1) vbuf[vused++] = *s++;
            vbuf[vused] = '\0';
            if (*s == '"') s++; /* cerrar comillas */
        } else {
            while (*s && !isspace((unsigned char)*s) && vused < sizeof(vbuf)-1) vbuf[vused++] = *s++;
            vbuf[vused] = '\0';
        }

        if (!strcmp(kbuf, "precio")) {
            precio = (float)atof(vbuf);
        } else if (!strcmp(kbuf, "stock")) {
            stock = atoi(vbuf);
        } else if (!strcmp(kbuf, "nombre") || !strcmp(kbuf, "producto")) {
            /* copiar valor base */
            size_t used = 0;
            nombre[0] = '\0';
            if (vbuf[0]) {
                size_t n = strlen(vbuf);
                if (n >= NOMBRE_MAXLEN) n = NOMBRE_MAXLEN - 1;
                memcpy(nombre, vbuf, n);
                nombre[n] = '\0';
                used = n;
            }

            if (!valor_comillado) {
                /* SIN comillas: pegar tokens sueltos del nombre hasta ver otro '=' */
                const char *look = s;
                while (1) {
                    /* mirar siguiente palabra */
                    while (*look && isspace((unsigned char)*look)) look++;
                    if (!*look) { s = look; break; }

                    const char *ls = look;
                    while (*look && !isspace((unsigned char)*look)) look++;
                    size_t llen = (size_t)(look - ls);
                    if (llen == 0) { s = look; break; }

                    /* copiar palabra a tmp y ver si es otra clave (contiene '=') */
                    char nxt[256];
                    if (llen >= sizeof(nxt)) llen = sizeof(nxt) - 1;
                    memcpy(nxt, ls, llen); nxt[llen] = '\0';

                    if (strchr(nxt, '=')) {
                        /* es otra clave -> no la consumimos aquí; dejamos s en ls para que el while exterior la lea */
                        s = ls;
                        break;
                    }

                    /* es parte del nombre -> la pegamos */
                    if (used < NOMBRE_MAXLEN - 1) {
                        nombre[used++] = ' ';
                        nombre[used] = '\0';
                    }
                    size_t add = strlen(nxt);
                    if (add > NOMBRE_MAXLEN - 1 - used) add = NOMBRE_MAXLEN - 1 - used;
                    memcpy(nombre + used, nxt, add);
                    used += add;
                    nombre[used] = '\0';

                    /* consumimos la palabra en s */
                    s = look;
                }
            } else {
                /* valor comillado ya quedó completo en vbuf */
                /* s ya apunta al primer espacio después de la comilla de cierre */
            }
        }

        /* sigue el loop para más k=v */
    }

    if (nombre[0] && precio >= 0.0f && stock >= 0) {
        size_t nlen = strlen(nombre);
        if (nlen >= NOMBRE_MAXLEN) nlen = NOMBRE_MAXLEN - 1;
        memcpy(nombre_out, nombre, nlen);
        nombre_out[nlen] = '\0';
        *precio_out = precio;
        *stock_out  = stock;
        return 0;
    }
    return -1;
}


/* =========================
   parseo de ADD (CSV o k=v)
   ========================= */
static int parse_add_any(const char *args, char *nombre_out, float *precio_out, int *stock_out) {
    /* Debug: log incoming args to help diagnose intermittent parsing failures */
    #if 1
    fprintf(stderr, "[PROTO] parse_add_any: args='%s'\n", args ? args : "(null)");
    #endif
    if (parse_add_csv(args, nombre_out, precio_out, stock_out) == 0) {
        fprintf(stderr, "[PROTO] parse_add_any: matched CSV\n");
        return 0;
    }
    if (parse_add_kv (args, nombre_out, precio_out, stock_out) == 0) {
        fprintf(stderr, "[PROTO] parse_add_any: matched KV\n");
        return 0;
    }
    fprintf(stderr, "[PROTO] parse_add_any: no match -> ERR ARG\n");
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

    /* snapshot de TX (quién es el dueño, etc.) — definidas en server.c */
    extern pthread_mutex_t tx_mtx;
    extern int tx_active;
    extern int tx_owner;

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
            /* sanitizar argumento */
            char argbuf[1024]; safe_copy(argbuf, sizeof(argbuf), pbuf); trim_and_unquote(argbuf);
            // Eliminar chequeo de transacción para FIND ALL
            if (buscar_nombre_todos(argbuf, &vec, &n) != 0) { dprintf(cfd, "END\n"); return; }
            for (size_t k = 0; k < n; ++k)
                dprintf(cfd, "ROW %d,%s,%.2f,%u,%s\n", vec[k].id, vec[k].nombre, vec[k].precio, vec[k].stock, vec[k].timestamp);
            free(vec);
            dprintf(cfd, "END\n");
            return;
        }
        // Eliminar chequeo de transacción para FIND
        char argbuf2[1024]; safe_copy(argbuf2, sizeof(argbuf2), pbuf); trim_and_unquote(argbuf2);
        if (buscar_nombre_primero(argbuf2, &r) == 0)
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

    char idbuf[64]; safe_copy(idbuf, sizeof(idbuf), pbuf); trim_and_unquote(idbuf);
    int id = atoi(idbuf);
        if (id <= 0) { dprintf(cfd, "ERR ARG\n"); return; }

        if (!eliminar_arch(id)) dprintf(cfd, "OK\n");
        else                    dprintf(cfd, "ERR NOT_FOUND\n");
        return;
    }

    /* ----------- QUIT ----------- */
    if (!strcmp(cmd, "QUIT")) { dprintf(cfd, "BYE\n"); return; }

    dprintf(cfd, "ERR CMD\n");
}
