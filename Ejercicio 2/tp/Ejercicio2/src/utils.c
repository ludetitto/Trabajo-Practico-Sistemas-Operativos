#include "utils.h"
#include <stdio.h>
#include <stdlib.h>

void perror_exit(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}


