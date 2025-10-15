// client.c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <poll.h>
#include <fcntl.h>
#ifdef __linux__
#include <netinet/tcp.h>
#endif

static volatile sig_atomic_t g_sigint_count = 0;

static void on_sigint(int sig)
{
    (void)sig;
    if (g_sigint_count < 2)
        g_sigint_count++;
}

static ssize_t escribir_todo(int fd, const void *buf, size_t n)
{
    const char *p = (const char *)buf;
    size_t faltan = n;
    while (faltan > 0)
    {
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

/* lee una línea del socket (hasta '\n'), 0 si peer cerró, -1 error */
static ssize_t leer_linea_sock(int fd, char *buf, size_t cap)
{
    size_t i = 0;
    while (i + 1 < cap)
    {
        char c;
        ssize_t r = read(fd, &c, 1);
        if (r == 0)
            break; // peer cerró
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

/* imprime respuesta; si empieza con ROW, consume hasta END */
static int leer_imprimir_respuesta(int fd)
{
    char buf[1024];
    ssize_t n = leer_linea_sock(fd, buf, sizeof(buf));
    if (n <= 0)
        return -1;

    printf("%s\n", buf);

    /* <- NUEVO: si el server dijo BYE, es cierre normal */
    if (!strcasecmp(buf, "BYE"))
        return 1; /* señal de cierre limpio */

    if (!strncasecmp(buf, "ROW ", 4))
    {
        for (;;)
        {
            n = leer_linea_sock(fd, buf, sizeof(buf));
            if (n <= 0)
                return -1;
            printf("%s\n", buf);
            if (!strcasecmp(buf, "END"))
                break;
        }
    }
    return 0;
}

static void habilitar_keepalive(int s)
{
    int ka = 1;
    (void)setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, &ka, sizeof(ka));
#ifdef __linux__
    int idle = 10, intvl = 3, cnt = 3;
    (void)setsockopt(s, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
    (void)setsockopt(s, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl));
    (void)setsockopt(s, IPPROTO_TCP, TCP_KEEPCNT, &cnt, sizeof(cnt));
#endif
}

int main(int argc, char **argv)
{
    const char *host = "127.0.0.1";
    int port = 5000;

    /* señales: Ctrl+C con salida ordenada */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);

    /* parse -H/-p (acepto -h como alias de -H por compatibilidad) */
    for (int i = 1; i < argc; i++)
    {
        if ((!strcmp(argv[i], "-H") || !strcmp(argv[i], "-h")) && i + 1 < argc)
            host = argv[++i];
        else if (!strcmp(argv[i], "-p") && i + 1 < argc)
            port = atoi(argv[++i]);
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        return 1;
    }
    habilitar_keepalive(sockfd);

    struct sockaddr_in sa_in;
    memset(&sa_in, 0, sizeof(sa_in));
    sa_in.sin_family = AF_INET;
    sa_in.sin_port = htons((uint16_t)port);
    sa_in.sin_addr.s_addr = inet_addr(host);

    if (connect(sockfd, (struct sockaddr *)&sa_in, sizeof(sa_in)) < 0)
    {
        perror("connect");
        close(sockfd);
        return 1;
    }

    /* banner inicial del server */
    {
        char buf[1024];
        if (leer_linea_sock(sockfd, buf, sizeof(buf)) > 0)
            printf("%s\n", buf);
        else
        {
            fprintf(stderr, "Servidor no envió banner y/o cerró.\n");
            close(sockfd);
            return 1;
        }
    }

    /* Poll: monitorear STDIN y el socket para detectar caída del server aun en idle */
    struct pollfd pfds[2];
    pfds[0].fd = 0;
    pfds[0].events = POLLIN; /* STDIN */
    pfds[1].fd = sockfd;
    pfds[1].events = POLLIN | POLLERR | POLLHUP;

    char linea[1024];
    int client_tx_active = 0;

    int running = 1;
    while (running)
    {
        /* 2do Ctrl+C = salir ya */
        if (g_sigint_count >= 2)
        {
            close(sockfd);
            return 130;
        }
        /* 1er Ctrl+C = rollback+quit ordenado */
        if (g_sigint_count == 1)
        {
            if (client_tx_active)
            {
                (void)escribir_todo(sockfd, "ROLLBACK\n", 9);
                client_tx_active = 0;
                /* respuesta opcional: (void)leer_imprimir_respuesta(sockfd); */
            }
            (void)escribir_todo(sockfd, "QUIT\n", 5);
            /* opcional: (void)leer_imprimir_respuesta(sockfd); */
            close(sockfd);
            return 130;
        }

        int pr = poll(pfds, 2, -1);
        if (pr < 0)
        {
            if (errno == EINTR)
                continue; /* re-evaluar Ctrl+C */
            perror("poll");
            break;
        }

        /* 1) Evento en socket: mensajes / caída del server */
        if (pfds[1].revents & (POLLERR | POLLHUP))
        {
            /* server murió o cerró */
            fprintf(stderr, "\n[CLIENTE] Conexión cerrada por el servidor.\n");
            close(sockfd);
            return 1;
        }
        if (pfds[1].revents & POLLIN)
        {
            int rr = leer_imprimir_respuesta(sockfd);
            if (rr < 0)
            {
                fprintf(stderr, "\n[CLIENTE] Conexión cerrada por el servidor.\n");
                close(sockfd);
                return 1;
            }
            if (rr > 0)
            { /* BYE */
                close(sockfd);
                return 0; /* <- salir OK */
            }
            continue;
        }

        /* 2) Evento en STDIN: leer comando del usuario */
        if (pfds[0].revents & POLLIN)
        {
            if (!fgets(linea, sizeof(linea), stdin))
            {
                /* EOF en stdin: cerrar prolijo */
                (void)escribir_todo(sockfd, "QUIT\n", 5);
                (void)leer_imprimir_respuesta(sockfd);
                break;
            }
            size_t len = strlen(linea);
            if (len == 0)
                continue;
            if (linea[len - 1] != '\n')
            {
                if (len + 1 < sizeof(linea))
                {
                    linea[len++] = '\n';
                    linea[len] = '\0';
                }
            }

            if (escribir_todo(sockfd, linea, len) < 0)
            {
                fprintf(stderr, "\n[CLIENTE] Error escribiendo al servidor (%s).\n", strerror(errno));
                close(sockfd);
                return 1;
            }
            /* No mostrar prompt aquí, solo tras respuesta del server */
        }
    }

    close(sockfd);
    return 0;
}
