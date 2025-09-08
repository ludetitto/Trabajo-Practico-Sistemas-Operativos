// include/producer.h
// -----------------------------------------------------------------------------
// Propósito:
// - Declara la rutina principal del proceso GENERADOR.
// - Cada generador pide bloques de IDs, arma registros (setea 'generador' y 'pid',
//   completa los campos de producto) y los deposita de a UNO en el ring buffer.
// -----------------------------------------------------------------------------

#ifndef GENERATOR_H
#define GENERATOR_H

#include "common.h"

void generator_loop(const names_t *nn, int generador_index);

#endif // GENERATOR_H
