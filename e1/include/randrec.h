#ifndef RANDREC_H
#define RANDREC_H
#include "common.h"

void randrec_seed(void);
void randrec_make(record_t* r, uint32_t id, int gen_idx);

#endif
