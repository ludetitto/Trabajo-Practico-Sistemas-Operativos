#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

/* escribe todo el buffer aunque write devuelva menos */
static ssize_t escribir_todo(int fd, const void *buf, size_t n) 
{
    const char *p = buf;
    size_t faltan = n;
    while (faltan > 0) {
        ssize_t r = write(fd, p, faltan);
        if (r < 0) 
        {
            if (errno == EINTR) 
            continue;
            return -1;
        }
        p += r;
        faltan -= (size_t)r;
    }
    return (ssize_t)n;
}

/* lee hasta '\n' o cap-1 */
static ssize_t leer_linea(int fd, char *buf, size_t cap) {
    size_t i = 0;
    while (i + 1 < cap) 
    {
        char c;
        ssize_t r = read(fd, &c, 1);
        if (r == 0) 
            break;      /* cerrado */
        if (r < 0) 
        {
            if (errno == EINTR) 
                continue;
            return -1;
        }
        if (c == '\n') 
            break;
        buf[i++] = c;
    }
    buf[i] = '\0';
    return (ssize_t)i;
}

int main(int argc, char **argv) 
{
    const char *host = "127.0.0.1";
    char linea[1024], buf[1024];
    int port = 5000, parar = 0, sockfd;
    struct sockaddr_in sa = {0};

    for (int i = 1; i < argc; i++) 
    {
        if (!strcmp(argv[i], "-h") && i + 1 < argc) 
            host = argv[++i];
        else if (!strcmp(argv[i], "-p") && i + 1 < argc) 
            port = atoi(argv[++i]);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
    { 
        perror("socket"); 
        return 1; 
    }

    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = inet_addr(host);

    if (connect(sockfd, (struct sockaddr *)&sa, sizeof(sa)) < 0) 
    {
        perror("connect");
        return 1;
    }

    /* banner inicial */
    if (leer_linea(sockfd, buf, sizeof(buf)) > 0)
        printf("%s\n", buf);

    /* bucle interactivo */
    while (parar > -1) 
    {
        size_t len;

        printf("> ");
        fflush(stdout);
        if (!fgets(linea, sizeof(linea), stdin)) 
            break;
        
        len = strlen(linea);
        if (len != 0)
        {
            if (linea[len - 1] != '\n') 
            linea[len++] = '\n';
            if ((parar = escribir_todo(sockfd, linea, len)) < 0) 
            {
                perror("write");
            }
            else
            {
                if (!strncasecmp(linea, "QUIT", 4)) break;
                if (leer_linea(sockfd, buf, sizeof(buf)) <= 0) break;
                printf("%s\n", buf);
            }
        }
    }

    close(sockfd);
    return 0;
}
