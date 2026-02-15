#define _POSIX_C_SOURCE 200809L
#include "protocol.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <strings.h>


int connect_to_server(const char *ip, int port); /* from client.c */

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <ip> <port>\n", argv[0]);
        return 1;
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);
    int s = connect_to_server(ip, port);

    char buf[2048];
    ssize_t n = recvline(s, buf, sizeof(buf));
    if (n > 0) printf("%s\n", buf);

    while (1) {
        printf("> ");
        if (!fgets(buf, sizeof(buf), stdin)) break;
        buf[strcspn(buf, "\n")] = '\0';
        if (strlen(buf) == 0) continue;
        sendline(s, "%s", buf);
        if (strcasecmp(buf, "QUIT") == 0 || strcasecmp(buf, "EXIT") == 0) break;
        /* read responses; LIST => multiple lines ended by END; others => single-line (server sends END after LIST) */
        while (1) {
            ssize_t r = recvline(s, buf, sizeof(buf));
            if (r <= 0) { printf("Disconnected\n"); goto done; }
            if (strcmp(buf, "END") == 0) break;
            printf("%s\n", buf);
            if (strncmp(buf, "OK", 2) == 0 || strncmp(buf, "ERR", 3) == 0 || strcmp(buf, "BYE") == 0 || strcmp(buf, "NOTFOUND") == 0) break;
        }
    }
done:
    close(s);
    return 0;
}






