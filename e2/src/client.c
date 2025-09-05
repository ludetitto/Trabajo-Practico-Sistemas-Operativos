#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

static ssize_t escribir_todo(int fd, const void *buf, size_t n) {
    const char *p = (const char *)buf;
    size_t faltan = n;
    while (faltan > 0) {
        ssize_t r = write(fd, p, faltan);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        p += r;
        faltan -= (size_t)r;
    }
    return (ssize_t)n;
}

static ssize_t leer_linea(int fd, char *buf, size_t cap) {
    size_t i = 0;
    while (i + 1 < cap) {
        char c;
        ssize_t r = read(fd, &c, 1);
        if (r == 0) break;                  // conexión cerrada
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        if (c == '\n') { buf[i] = '\0'; return (ssize_t)i; }
        buf[i++] = c;
    }
    buf[i] = '\0';
    return (ssize_t)i;
}

static void quitar_nl(char *s) {
    size_t n = strlen(s);
    if (n && s[n-1] == '\n') s[n-1] = '\0';
}

int main(int argc, char **argv)
{
    const char *direccion_host = "127.0.0.1";
    int puerto = 5000;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h") && i + 1 < argc)
            direccion_host = argv[++i];
        else if (!strcmp(argv[i], "-p") && i + 1 < argc)
            puerto = atoi(argv[++i]);
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { perror("socket"); return 1; }

    struct sockaddr_in dir = {0};
    dir.sin_family = AF_INET;
    dir.sin_port = htons(puerto);
    dir.sin_addr.s_addr = inet_addr(direccion_host);

    if (connect(sockfd, (struct sockaddr *)&dir, sizeof(dir)) < 0) {
        perror("connect");
        return 1;
    }

    // Leer banner inicial del servidor (si lo hay)
    char buffer[1024];
    ssize_t n = leer_linea(sockfd, buffer, sizeof(buffer));
    if (n > 0) {
        fputs(buffer, stdout);
        fputc('\n', stdout);
    }

    // REPL: leo de stdin, envío al servidor y muestro respuesta
    char linea[1024];
    for (;;) {
        fputs("> ", stdout);
        fflush(stdout);

        if (!fgets(linea, sizeof(linea), stdin)) break;  // EOF o error
        quitar_nl(linea);
        if (linea[0] == '\0') continue;                  // línea vacía, seguir

        // armo comando + '\n' para el servidor
        size_t len = strlen(linea);
        linea[len] = '\n';
        linea[len+1] = '\0';

        if (escribir_todo(sockfd, linea, len + 1) < 0) {
            perror("write");
            break;
        }

        // leer 1 línea de respuesta (ajusta si tu protocolo devuelve múltiples)
        n = leer_linea(sockfd, buffer, sizeof(buffer));
        if (n <= 0) {            // cerrado o error
            if (n < 0) perror("read");
            break;
        }
        fputs(buffer, stdout);
        fputc('\n', stdout);

        // si el servidor confirma cierre
        if (!strcasecmp(linea, "QUIT\n")) break;
    }

    close(sockfd);
    return 0;
}
