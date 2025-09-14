#include <stdio.h>
#include <stdlib.h>
#include "csv_db.h"

/* prototype from server.c */
void run_server(const char *ip, int port, int max_clients, int backlog);

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <ip> <port> <max_clients> <backlog>\n", argv[0]);
        return 1;
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);
    int max_clients = atoi(argv[3]);
    int backlog = atoi(argv[4]);
    run_server(ip, port, max_clients, backlog);
    return 0;
}
