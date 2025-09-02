#include <stdlib.h>
#include <time.h>
#include "../include/eventos.h"

const char *evento_aleatorio(void)
{
    static const char *g_eventos[] = {
        "Lollapalooza", "Cosquin Rock", "Bresh"};
    static int seeded = 0;
    if (!seeded)
    {
        srand((unsigned)time(NULL) ^ (unsigned)clock());
        seeded = 1;
    }
    size_t n = sizeof(g_eventos) / sizeof(g_eventos[0]);
    return g_eventos[rand() % n];
}
