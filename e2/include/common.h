#ifndef COMMON_H
#define COMMON_H

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define NAME_MAXLEN 32
#define LINE_MAX    1024

static inline void die(const char* fmt, ...) {
  va_list ap; va_start(ap, fmt);
  vfprintf(stderr, fmt, ap); va_end(ap);
  fputc('\n', stderr);
  exit(EXIT_FAILURE);
}
static inline void trim_nl(char *s){
  if(!s) return;
  size_t n=strlen(s); if(n && (s[n-1]=='\n' || s[n-1]=='\r')) s[n-1]=0;
}

#endif
