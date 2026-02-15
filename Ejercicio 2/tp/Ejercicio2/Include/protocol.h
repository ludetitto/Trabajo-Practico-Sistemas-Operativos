#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <sys/types.h>

/* Envío/recepción de líneas terminadas en '\n' */
ssize_t sendline(int fd, const char *fmt, ...);
ssize_t recvline(int fd, char *buf, size_t bufsz);

#endif




