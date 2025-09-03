// include/producer.h
// -----------------------------------------------------------------------------
// Proceso generador: toma bloques de IDs y produce registros aleatorios,
// depositándolos en el ring buffer de a UNO (requisito).
// -----------------------------------------------------------------------------

#ifndef PRODUCER_H
#define PRODUCER_H

#include "common.h"

void generator_loop(const names_t *nn);

#endif // PRODUCER_H
