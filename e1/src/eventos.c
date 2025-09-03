#include <stdlib.h>
#include <time.h>
#include "../include/eventos.h"

const char *evento_aleatorio(void)
{
    static const char *eventos[] = {
        "Lollapalooza", "Cosquin Rock", "Bresh"};
    static int semilla = 0;
    if (!semilla)
    {
        srand((unsigned)time(NULL) ^ (unsigned)clock());
        semilla = 1;
    }
    size_t n = sizeof(eventos) / sizeof(eventos[0]);
    return eventos[rand() % n];
}
