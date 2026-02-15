#define _POSIX_C_SOURCE 200809L
#include "csv_db.h"
#include "protocol.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <strings.h>


/* concurrency primitives */
static pthread_rwlock_t db_lock = PTHREAD_RWLOCK_INITIALIZER; /* readers/writer lock */
static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
static int active_clients = 0;

/* server running flag and listen fd for graceful shutdown */
static volatile sig_atomic_t running = 1;
static int listen_fd_global = -1;

/* db path (set from argv) */
static char db_path[512] = "data/database.csv";

/* Transaction operation buffer types */
typedef enum { OP_INSERT, OP_UPDATE, OP_DELETE } OpType;
typedef struct {
    OpType type;
    char name[MAX_NAME];
    double price;
    int stock;
    int id;
    char field[32];
    char value[128];
} TxOp;

typedef struct {
    TxOp *ops;
    size_t len;
    size_t cap;
    int dirty;
} TxBuffer;

static void tx_init(TxBuffer *tx) { tx->ops = NULL; tx->len = tx->cap = 0; tx->dirty = 0; }
static void tx_free(TxBuffer *tx) { free(tx->ops); tx->ops = NULL; tx->len = tx->cap = 0; tx->dirty = 0; }
static int tx_push(TxBuffer *tx, TxOp op) {
    if (tx->len == tx->cap) {
        size_t nc = tx->cap ? tx->cap * 2 : 8;
        TxOp *p = realloc(tx->ops, nc * sizeof(TxOp));
        if (!p) return -1;
        tx->ops = p; tx->cap = nc;
    }
    tx->ops[tx->len++] = op;
    tx->dirty = 1;
    return 0;
}

/* helper trim */
static void trim(char *s) {
    char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n-1])) s[--n] = '\0';
}

static void signal_handler(int sig) {
    (void)sig;
    running = 0;
    if (listen_fd_global >= 0) close(listen_fd_global);
}

/* handlers for commands (outside/inside tx) */
static void handle_get(int client_fd, const char *arg) {
    int id = atoi(arg);
    if (pthread_rwlock_tryrdlock(&db_lock) != 0) {
        sendline(client_fd, "ERR TRANSACTION_ACTIVE");
        return;
    }
    Product p;
    if (db_get_by_id(id, &p) != 0) {
        pthread_rwlock_unlock(&db_lock);
        sendline(client_fd, "NOTFOUND");
        return;
    }
    char out[512];
    snprintf(out, sizeof(out), "%d,%s,%.2f,%d,%d,%s", p.id, p.name, p.price, p.stock, p.generator_pid, p.ts);
    pthread_rwlock_unlock(&db_lock);
    sendline(client_fd, "%s", out);
}

static void handle_list(int client_fd) {
    if (pthread_rwlock_tryrdlock(&db_lock) != 0) {
        sendline(client_fd, "ERR TRANSACTION_ACTIVE");
        return;
    }
    char buf[8192];
    size_t used = 0;
    if (db_list_all(buf, sizeof(buf), &used) == 0 && used > 0) {
        char *p = buf;
        char *nl;
        while ((nl = strchr(p, '\n')) != NULL) {
            *nl = '\0';
            sendline(client_fd, "%s", p);
            p = nl + 1;
        }
    }
    pthread_rwlock_unlock(&db_lock);
    sendline(client_fd, "END");
}

static void handle_insert_autocommit(int client_fd, const char *args) {
    /* parse: <name> <price> <stock> */
    char name[128];
    double price;
    int stock;
    if (sscanf(args, "%127s %lf %d", name, &price, &stock) != 3) {
        sendline(client_fd, "ERR BAD_FORMAT");
        return;
    }
    if (pthread_rwlock_trywrlock(&db_lock) != 0) {
        sendline(client_fd, "ERR TRANSACTION_ACTIVE");
        return;
    }
    int nid;
    db_insert_product(name, price, stock, &nid);
    if (db_write_atomic(db_path) != 0) {
        pthread_rwlock_unlock(&db_lock);
        sendline(client_fd, "ERR WRITE_FAIL");
        return;
    }
    pthread_rwlock_unlock(&db_lock);
    sendline(client_fd, "OK %d", nid);
}

static void handle_update_autocommit(int client_fd, const char *args) {
    /* args: <id> <field> <value> */
    int id;
    char field[32];
    char value[128];
    if (sscanf(args, "%d %31s %127[^\n]", &id, field, value) < 3) {
        sendline(client_fd, "ERR BAD_ARGS");
        return;
    }
    trim(value);
    if (pthread_rwlock_trywrlock(&db_lock) != 0) {
        sendline(client_fd, "ERR TRANSACTION_ACTIVE");
        return;
    }
    int r = db_update_field(id, field, value);
    if (r == -1) { pthread_rwlock_unlock(&db_lock); sendline(client_fd, "ERR NOTFOUND"); return; }
    if (r == -2) { pthread_rwlock_unlock(&db_lock); sendline(client_fd, "ERR BAD_FIELD"); return; }
    if (db_write_atomic(db_path) != 0) { pthread_rwlock_unlock(&db_lock); sendline(client_fd, "ERR WRITE_FAIL"); return; }
    pthread_rwlock_unlock(&db_lock);
    sendline(client_fd, "OK");
}

static void handle_delete_autocommit(int client_fd, const char *args) {
    int id = atoi(args);
    if (pthread_rwlock_trywrlock(&db_lock) != 0) {
        sendline(client_fd, "ERR TRANSACTION_ACTIVE");
        return;
    }
    if (db_delete(id) != 0) { pthread_rwlock_unlock(&db_lock); sendline(client_fd, "ERR NOTFOUND"); return; }
    if (db_write_atomic(db_path) != 0) { pthread_rwlock_unlock(&db_lock); sendline(client_fd, "ERR WRITE_FAIL"); return; }
    pthread_rwlock_unlock(&db_lock);
    sendline(client_fd, "OK");
}

/* client thread */
static void *client_thread(void *arg) {
    int client_fd = *(int*)arg;
    free(arg);

    TxBuffer tx; tx_init(&tx);
    int in_tx = 0;

    sendline(client_fd, "WELCOME MicroDB");

    char line[1024];
    while (1) {
        ssize_t r = recvline(client_fd, line, sizeof(line));
        if (r <= 0) break; /* client closed or error */
        trim(line);
        if (line[0] == '\0') continue;

        /* parse command (separa primera palabra) */
        char cmdbuf[64];
        strncpy(cmdbuf, line, sizeof(cmdbuf)-1); cmdbuf[sizeof(cmdbuf)-1] = '\0';
        char *space = strchr(cmdbuf, ' ');
        if (space) *space = '\0';
        char *rest = NULL;
        char *first_space = strchr(line, ' ');
        if (first_space) rest = first_space + 1;

        if (strcasecmp(cmdbuf, "QUIT") == 0 || strcasecmp(cmdbuf, "EXIT") == 0) {
            sendline(client_fd, "BYE");
            break;
        }

        /* OUTSIDE TRANSACTION commands */
        if (!in_tx) {
            if (strcasecmp(cmdbuf, "GET") == 0) {
                if (!rest) { sendline(client_fd, "ERR MISSING_ID"); continue; }
                handle_get(client_fd, rest);
                continue;
            } else if (strcasecmp(cmdbuf, "LIST") == 0) {
                handle_list(client_fd);
                continue;
            } else if (strcasecmp(cmdbuf, "INSERT") == 0) {
                if (!rest) { sendline(client_fd, "ERR BAD_ARGS"); continue; }
                handle_insert_autocommit(client_fd, rest);
                continue;
            } else if (strcasecmp(cmdbuf, "UPDATE") == 0) {
                if (!rest) { sendline(client_fd, "ERR BAD_ARGS"); continue; }
                handle_update_autocommit(client_fd, rest);
                continue;
            } else if (strcasecmp(cmdbuf, "DELETE") == 0) {
                if (!rest) { sendline(client_fd, "ERR BAD_ARGS"); continue; }
                handle_delete_autocommit(client_fd, rest);
                continue;
            } else if (strcasecmp(line, "BEGIN TRANSACTION") == 0 || strcasecmp(line, "BEGIN") == 0) {
                /* Acquire exclusive writer lock blocking */
                pthread_rwlock_wrlock(&db_lock);
                in_tx = 1;
                tx_init(&tx);
                sendline(client_fd, "OK: BEGIN");
                continue;
            }
        } else {
            /* IN TRANSACTION: accept INSERT/UPDATE/DELETE as queued ops, COMMIT, ROLLBACK, and GET/LIST should read from in-memory DB (we already hold wrlock so safe) */
            if (strcasecmp(cmdbuf, "INSERT") == 0) {
                if (!rest) { sendline(client_fd, "ERR BAD_ARGS"); continue; }
                char name[128]; double price; int stock;
                if (sscanf(rest, "%127s %lf %d", name, &price, &stock) != 3) { sendline(client_fd, "ERR BAD_FORMAT"); continue; }
                TxOp op; memset(&op,0,sizeof(op));
                op.type = OP_INSERT;
                strncpy(op.name, name, sizeof(op.name)-1);
                op.price = price;
                op.stock = stock;
                if (tx_push(&tx, op) != 0) { sendline(client_fd, "ERR OOM"); continue; }
                sendline(client_fd, "OK (queued)");
                continue;
            } else if (strcasecmp(cmdbuf, "UPDATE") == 0) {
                if (!rest) { sendline(client_fd, "ERR BAD_ARGS"); continue; }
                int id; char field[32]; char value[128];
                if (sscanf(rest, "%d %31s %127[^\n]", &id, field, value) < 3) { sendline(client_fd, "ERR BAD_ARGS"); continue; }
                trim(value);
                TxOp op; memset(&op,0,sizeof(op));
                op.type = OP_UPDATE;
                op.id = id;
                strncpy(op.field, field, sizeof(op.field)-1);
                strncpy(op.value, value, sizeof(op.value)-1);
                if (tx_push(&tx, op) != 0) { sendline(client_fd, "ERR OOM"); continue; }
                sendline(client_fd, "OK (queued)");
                continue;
            } else if (strcasecmp(cmdbuf, "DELETE") == 0) {
                if (!rest) { sendline(client_fd, "ERR BAD_ARGS"); continue; }
                int id = atoi(rest);
                TxOp op; memset(&op,0,sizeof(op));
                op.type = OP_DELETE;
                op.id = id;
                if (tx_push(&tx, op) != 0) { sendline(client_fd, "ERR OOM"); continue; }
                sendline(client_fd, "OK (queued)");
                continue;
            } else if (strcasecmp(line, "COMMIT TRANSACTION") == 0 || strcasecmp(line, "COMMIT") == 0) {
                /* we already hold wrlock; apply queued ops directly to in-memory DB */
                int ok = 1;
                for (size_t i = 0; i < tx.len; ++i) {
                    TxOp *op = &tx.ops[i];
                    if (op->type == OP_INSERT) {
                        if (db_insert_product(op->name, op->price, op->stock, NULL) != 0) { ok = 0; break; }
                    } else if (op->type == OP_UPDATE) {
                        int r = db_update_field(op->id, op->field, op->value);
                        if (r != 0) { ok = 0; break; }
                    } else if (op->type == OP_DELETE) {
                        if (db_delete(op->id) != 0) { ok = 0; break; }
                    }
                }
                if (!ok) {
                    db_load(db_path); /* rollback by reloading from disk */
                    tx_free(&tx);
                    in_tx = 0;
                    pthread_rwlock_unlock(&db_lock);
                    sendline(client_fd, "ERR COMMIT_FAILED");
                    continue;
                }
                if (db_write_atomic(db_path) != 0) {
                    db_load(db_path);
                    tx_free(&tx);
                    in_tx = 0;
                    pthread_rwlock_unlock(&db_lock);
                    sendline(client_fd, "ERR WRITE_FAIL");
                    continue;
                }
                tx_free(&tx);
                in_tx = 0;
                pthread_rwlock_unlock(&db_lock);
                sendline(client_fd, "OK: COMMIT");
                continue;
            } else if (strcasecmp(line, "ROLLBACK TRANSACTION") == 0 || strcasecmp(line, "ROLLBACK") == 0) {
                tx_free(&tx);
                in_tx = 0;
                pthread_rwlock_unlock(&db_lock);
                sendline(client_fd, "OK: ROLLBACK");
                continue;
            } else {
                /* While in transaction, GET/LIST are allowed because we hold write lock; handle them */
                if (strcasecmp(cmdbuf, "GET") == 0) {
                    if (!rest) { sendline(client_fd, "ERR MISSING_ID"); continue; }
                    handle_get(client_fd, rest);
                    continue;
                } else if (strcasecmp(cmdbuf, "LIST") == 0) {
                    handle_list(client_fd);
                    continue;
                } else {
                    sendline(client_fd, "ERR UNKNOWN_COMMAND");
                    continue;
                }
            }
        }

        /* If reached here, the command was not recognized in any branch */
        sendline(client_fd, "ERR UNKNOWN_COMMAND");
    }

    /* cleanup on disconnect */
    if (in_tx) {
        tx_free(&tx);
        pthread_rwlock_unlock(&db_lock);
    }

    pthread_mutex_lock(&clients_mutex);
    active_clients--;
    pthread_mutex_unlock(&clients_mutex);

    close(client_fd);
    return NULL;
}

/* start server loop */
void run_server(const char *ip, int port, int max_clients, int backlog) {
    strncpy(db_path, "data/database.csv", sizeof(db_path)-1);
    db_load(db_path);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) perror_exit("socket");
    listen_fd_global = listen_fd;

    int on = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);

    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) perror_exit("bind");
    if (listen(listen_fd, backlog) < 0) perror_exit("listen");

    printf("Server listening %s:%d (max_clients=%d backlog=%d)\n", ip, port, max_clients, backlog);

    while (running) {
        struct sockaddr_in cli;
        socklen_t clen = sizeof(cli);
        int client_fd = accept(listen_fd, (struct sockaddr*)&cli, &clen);
        if (client_fd < 0) {
            if (!running && errno == EBADF) break;
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        /* enforce max_clients */
        pthread_mutex_lock(&clients_mutex);
        if (active_clients >= max_clients) {
            pthread_mutex_unlock(&clients_mutex);
            sendline(client_fd, "ERR MAX_CLIENTS");
            close(client_fd);
            continue;
        }
        active_clients++;
        pthread_mutex_unlock(&clients_mutex);

        int *pfd = malloc(sizeof(int));
        *pfd = client_fd;
        pthread_t tid;
        pthread_create(&tid, NULL, client_thread, pfd);
        pthread_detach(tid);
    }

    close(listen_fd);
    db_free();
    printf("Server shutdown\n");
}




