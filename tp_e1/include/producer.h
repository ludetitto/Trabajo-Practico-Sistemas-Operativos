// include/producer.h
// -----------------------------------------------------------------------------
// Proceso GENERADOR: solicita bloques de IDs y PRIORIDADES, arma registros y
// los deposita de a uno en el ring buffer.
// -----------------------------------------------------------------------------

#ifndef PRODUCER_H
#define PRODUCER_H

#include "common.h"

void generator_loop(const names_t *nn);

#endif // PRODUCER_H
