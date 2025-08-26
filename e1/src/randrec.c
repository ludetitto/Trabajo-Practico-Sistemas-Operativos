#include "../include/randrec.h"

static const char* NAMES[] = {
    "Luis","Marta","Juan","Sofia","Pedro","Ana","Diego","Clara","Beta","Omega","Kappa","Alpha","Gamma","Delta","Lambda"
};
static const size_t N_NAMES = sizeof(NAMES)/sizeof(NAMES[0]);

void randrec_seed(void) {
    static int seeded = 0;
    if (!seeded) { srand((unsigned)time(NULL) ^ (unsigned)getpid()); seeded = 1; }
}

void randrec_make(record_t* r, uint32_t id, int gen_idx) {
    randrec_seed();
    memset(r, 0, sizeof(*r));
    r->id = id;
    r->generador = gen_idx;
    r->pid = getpid();
    strncpy(r->nombre, NAMES[rand() % N_NAMES], NAME_MAXLEN-1);
}
