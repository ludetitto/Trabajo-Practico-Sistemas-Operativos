#define _POSIX_C_SOURCE 200809L
#include "protocol.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

/* sendline: format + '\n' */
ssize_t sendline(int fd, const char *fmt, ...) {
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    size_t len = strlen(buf);
    if (len + 1 >= sizeof(buf)) len = sizeof(buf) - 2;
    buf[len++] = '\n';
    return send(fd, buf, len, 0);
}

/* recvline: read until '\n' or EOF; nul-terminate; returns bytes read (without newline) */
ssize_t recvline(int fd, char *buf, size_t bufsz) {
    size_t pos = 0;
    while (pos + 1 < bufsz) {
        char c;
        ssize_t r = recv(fd, &c, 1, 0);
        if (r == 0) { /* closed */
            if (pos == 0) return 0;
            break;
        }
        if (r < 0) return -1;
        if (c == '\r') continue;
        if (c == '\n') break;
        buf[pos++] = c;
    }
    buf[pos] = '\0';
    return (ssize_t)pos;
}


