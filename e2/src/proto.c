// proto.c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include "../include/proto.h"
#include "../include/csvdb.h"

/* --- Helpers de parsing y limpieza --- */
static void rstrip(char *s) {                 // quita espacios finales
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n-1])) s[--n] = 0;
}
static void lskip(const char **ps) {          // salta espacios iniciales
    const char *p = *ps; while (*p && isspace((unsigned char)*p)) p++; *ps = p;
}
static void upper(char *s) {                  // a MAYÚSCULAS
    for (; *s; s++) *s = (char)toupper((unsigned char)*s);
}

/* --- Intérprete del protocolo línea a línea --- */
void procesar_linea_protocolo(int cfd, const char *linea)
{
    char buf[1024], cmd[32] = {0};
    const char *pbuf;
    int i = 0, local_tx_active, local_tx_owner;
    registro_t *vec = NULL, r;
    size_t n = 0;

    strncpy(buf, linea, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    rstrip(buf);
    pbuf = buf; lskip(&pbuf);
    if (!*pbuf) { dprintf(cfd, "ERR EMPTY\n"); return; }

    while (pbuf[i] && !isspace((unsigned char)pbuf[i]) && i < (int)sizeof(cmd) - 1) {
        cmd[i] = pbuf[i]; i++;
    }
    cmd[i] = 0; upper(cmd); pbuf += i; lskip(&pbuf);

    /* snapshot de TX: para política de bloqueo */
    pthread_mutex_lock(&tx_mtx);
    local_tx_active = tx_active;
    local_tx_owner  = tx_owner;
    pthread_mutex_unlock(&tx_mtx);

    /* PING */
    if (!strcmp(cmd, "PING")) { dprintf(cfd, "OK\n"); return; }

    /* GET <id> (lectura permitida si no hay TX ajena) */
    if (!strcmp(cmd, "GET")) {
        int id = atoi(pbuf);

        if (local_tx_active && local_tx_owner != cfd) { dprintf(cfd, "ERR TX_ACTIVE\n"); return; }

        if (!buscar_id_arch(id, &r))
            dprintf(cfd, "RESULT %d,%s,%.2f,%u,%s\n",
                    r.id, r.nombre, r.precio, r.stock, r.timestamp);
        else
            dprintf(cfd, "ERR NOT_FOUND\n");
        return;
    }

    /* FIND / FIND ALL: búsqueda por nombre (case-insensitive) */
    if (!strcmp(cmd, "FIND")) {
        // ¿FIND ALL ... ?
        if (!strncasecmp(pbuf, "ALL", 3) && isspace((unsigned char)pbuf[3])) {
            pbuf += 3; while (*pbuf && isspace((unsigned char)*pbuf)) pbuf++;

            if (local_tx_active && local_tx_owner != cfd) { dprintf(cfd, "ERR TX_ACTIVE\n"); return; }

            if (buscar_nombre_todos(pbuf, &vec, &n) != 0) {
                dprintf(cfd, "END\n");       // cerrar con END aunque no haya filas
                return;
            }
            for (size_t k = 0; k < n; ++k) {
                dprintf(cfd, "ROW %d,%s,%.2f,%u,%s\n",
                        vec[k].id, vec[k].nombre, vec[k].precio, vec[k].stock, vec[k].timestamp);
            }
            free(vec);
            dprintf(cfd, "END\n");           // cierre de batch
            return;
        }

        // FIND simple
        if (local_tx_active && local_tx_owner != cfd) { dprintf(cfd, "ERR TX_ACTIVE\n"); return; }

        if (buscar_nombre_primero(pbuf, &r) == 0)
            dprintf(cfd, "RESULT %d,%s,%.2f,%u,%s\n",
                    r.id, r.nombre, r.precio, r.stock, r.timestamp);
        else
            dprintf(cfd, "ERR NOT_FOUND\n");
        return;
    }

    /* MODIFY <id> (NOMBRE|PRECIO|STOCK) <valor> */
    if (!strcmp(cmd, "MODIFY")) {
        /* Bloqueo: permitir si no hay TX ajena; no exige TX propia porque es DML,
           pero si tu política lo requiere, podés exigir TX como en UPDATE/ADD. */
        if (local_tx_active && local_tx_owner != cfd) {
            dprintf(cfd, "ERR TX_ACTIVE\n");
            return;
        }

        // Leer <id>
        int id = atoi(pbuf);
        // avanzar pbuf más allá del id
        while (*pbuf && !isspace((unsigned char)*pbuf)) pbuf++;
        while (*pbuf && isspace((unsigned char)*pbuf)) pbuf++;

        if (!strncasecmp(pbuf, "NOMBRE", 6)) {
            pbuf += 6; while (*pbuf && isspace((unsigned char)*pbuf)) pbuf++;
            if (modificar_nombre_id(id, pbuf, &r) == 0) {
                dprintf(cfd, "MODIFICADO %d,%s,%.2f,%u,%s\n",
                        r.id, r.nombre, r.precio, r.stock, r.timestamp);
            } else {
                dprintf(cfd, "ERR NOT_FOUND\n");
            }
            return;
        } else if (!strncasecmp(pbuf, "PRECIO", 6)) {
            pbuf += 6; while (*pbuf && isspace((unsigned char)*pbuf)) pbuf++;
            float precio = (float)atof(pbuf);
            if (modificar_precio_id(id, precio, &r) == 0) {
                dprintf(cfd, "MODIFICADO %d,%s,%.2f,%u,%s\n",
                        r.id, r.nombre, r.precio, r.stock, r.timestamp);
            } else {
                dprintf(cfd, "ERR NOT_FOUND\n");
            }
            return;
        } else if (!strncasecmp(pbuf, "STOCK", 5)) {
            pbuf += 5; while (*pbuf && isspace((unsigned char)*pbuf)) pbuf++;
            uint32_t stock = (uint32_t)atoi(pbuf);
            if (modificar_stock_id(id, stock, &r) == 0) {
                dprintf(cfd, "MODIFICADO %d,%s,%.2f,%u,%s\n",
                        r.id, r.nombre, r.precio, r.stock, r.timestamp);
            } else {
                dprintf(cfd, "ERR NOT_FOUND\n");
            }
            return;
        } else {
            dprintf(cfd, "ERR COMMAND_NOT_FOUND\n");
            return;
        }
    }

    /* ====== DML requieren TX activa y ser dueño ====== */
    if (!strcmp(cmd, "ADD")) {
        registro_t rr = {0};
        char nombre[NOMBRE_MAXLEN]; float precio = 0; int stock = 0;
        const time_t ahora = time(NULL);

        if (!local_tx_active)           { dprintf(cfd, "ERR NOT_TX_ACTIVE\n"); return; }
        if (local_tx_owner != cfd)      { dprintf(cfd, "ERR TX_ACTIVE\n");    return; }

        if (sscanf(pbuf, "%63[^,],%f,%d", nombre, &precio, &stock) < 3) {
            dprintf(cfd, "ERR ARG\n");
            return;
        }

        strncpy(rr.nombre, nombre, NOMBRE_MAXLEN - 1); rr.nombre[NOMBRE_MAXLEN - 1] = '\0';
        rr.precio = precio; rr.stock = stock;
        strftime(rr.timestamp, sizeof(rr.timestamp), "%Y-%m-%d %H:%M:%S", localtime(&ahora));
        rr.borrado = false;

        if (!agregar_arch(&rr)) dprintf(cfd, "OK\n");
        else                    dprintf(cfd, "ERR IO\n");
        return;
    }

    if (!strcmp(cmd, "UPDATE")) {
        int id; char nombre[NOMBRE_MAXLEN]; float precio; int stock;
        registro_t patch = {0};

        if (!local_tx_active)           { dprintf(cfd, "ERR NOT_TX_ACTIVE\n"); return; }
        if (local_tx_owner != cfd)      { dprintf(cfd, "ERR TX_ACTIVE\n");    return; }

        if (sscanf(pbuf, "%d,%63[^,],%f,%d", &id, nombre, &precio, &stock) < 4) {
            dprintf(cfd, "ERR ARG\n");
            return;
        }

        patch.id = id;
        memcpy(patch.nombre, nombre, NOMBRE_MAXLEN - 1); patch.nombre[NOMBRE_MAXLEN - 1] = '\0';
        patch.precio = precio; patch.stock = (uint32_t)stock;

        if (!actualizar_arch(&patch)) dprintf(cfd, "OK\n");
        else                          dprintf(cfd, "ERR NOT_FOUND\n");
        return;
    }

    if (!strcmp(cmd, "DELETE")) {
        int id = atoi(pbuf);

        if (!local_tx_active)           { dprintf(cfd, "ERR NOT_TX_ACTIVE\n"); return; }
        if (local_tx_owner != cfd)      { dprintf(cfd, "ERR TX_ACTIVE\n");    return; }
        if (id <= 0)                    { dprintf(cfd, "ERR ARG\n");          return; }

        if (!eliminar_arch(id)) dprintf(cfd, "OK\n");
        else                    dprintf(cfd, "ERR NOT_FOUND\n");
        return;
    }

    if (!strcmp(cmd, "QUIT")) { dprintf(cfd, "BYE\n"); return; }

    dprintf(cfd, "ERR CMD\n");
}
