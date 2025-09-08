#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <pthread.h>
#include <unistd.h>
#include "../include/proto.h"
#include "../include/csvdb.h"

static void rstrip(char *s) // Permite eliminar espacios en blanco a la derecha.
{
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n-1])) 
        s[--n] = 0;
}
static void lskip(const char **ps) // Permite eliminar espacios en blanco a la izquierda.
{
    const char *p = *ps;
    while (*p && isspace((unsigned char)*p)) 
        p++;
    *ps = p;
}
static void upper(char *s) 
{
    for (; *s; s++) *s = (char)toupper((unsigned char)*s);
}

void procesar_linea_protocolo(int cfd, const char *linea) 
{
    char buf[1024], cmd[32] = {0};
    const char *pbuf;
    int i = 0;
    strncpy(buf, linea, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    rstrip(buf);

    pbuf = buf;
    lskip(&pbuf);
    if (!*pbuf) 
    { 
        dprintf(cfd, "ERR EMPTY\n"); 
        return; 
    }
    
    while (pbuf[i] && !isspace((unsigned char)pbuf[i]) && i < (int)sizeof(cmd) - 1) 
    {
        cmd[i] = pbuf[i]; 
        i++;
    }

    cmd[i] = 0;
    upper(cmd);
    pbuf += i; lskip(&pbuf);

    if (!strcmp(cmd, "PING")) 
    {
        dprintf(cfd, "OK\n");
        return;
    }
    if (!strcmp(cmd, "GET")) 
    {
        int id = atoi(pbuf);
        registro_t r;
        if (buscar_id_arch(id, &r) == 0)
            dprintf(cfd, "RESULT %d,%s,%.2f,%u,%s\n",
                    r.id, r.nombre, r.precio, r.stock, r.timestamp);
        else
            dprintf(cfd, "ERR NOT_FOUND\n");
        return;
    }
    if (!strcmp(cmd, "ADD")) 
    {
        registro_t r = {0};
        char nombre[NOMBRE_MAXLEN]; 
        float precio = 0; 
        int stock = 0;
        if (sscanf(pbuf, "%63[^,],%f,%d", nombre, &precio, &stock) < 3) 
        {
            dprintf(cfd, "ERR ARG\n"); 
            return;
        }
        strncpy(r.nombre, nombre, NOMBRE_MAXLEN - 1);
        r.nombre[NOMBRE_MAXLEN - 1] = '\0';
        r.precio = precio;
        r.stock = stock;
        snprintf(r.timestamp, sizeof(r.timestamp), "now"); // TODO: fecha real
        r.borrado = false;

        if (!agregar_arch(&r)) 
            dprintf(cfd, "OK\n");
        else 
            dprintf(cfd, "ERR IO\n");
        return;
    }
    if (!strcmp(cmd, "UPDATE")) 
    {
        int id; char nombre[NOMBRE_MAXLEN]; 
        float precio; 
        int stock;

        if (sscanf(pbuf, "%d,%63[^,],%f,%d", &id, nombre, &precio, &stock) < 4) 
        {
            dprintf(cfd, "ERR ARG\n"); 
            return;
        }
        registro_t patch = {0};
        patch.id = id;
        memcpy(patch.nombre, nombre, NOMBRE_MAXLEN - 1);
        patch.nombre[NOMBRE_MAXLEN - 1] = '\0';
        patch.precio = precio;
        patch.stock = stock;

        if (!actualizar_arch(&patch)) 
            dprintf(cfd, "OK\n");
        else 
            dprintf(cfd, "ERR NOT_FOUND\n");
        return;
    }
    if (!strcmp(cmd, "DELETE")) 
    {
        int id = atoi(pbuf);
        if (id <= 0)
        {
            dprintf(cfd, "ERR ARG\n"); 
            return; 
        }
        if (!eliminar_arch(id))
            dprintf(cfd, "OK\n");
        else 
            dprintf(cfd, "ERR NOT_FOUND\n");
        return;
    }
    if (!strcmp(cmd, "QUIT")) 
    {
        dprintf(cfd, "BYE\n");
        return;
    }

    dprintf(cfd, "ERR CMD\n");
}
