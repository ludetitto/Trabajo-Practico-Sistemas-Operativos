// include/coordinator.h
// -----------------------------------------------------------------------------
// Propósito:
// - Declara la rutina del COORDINADOR (consumidor).
// - Extrae EXACTAMENTE 'total' registros del ring y los escribe al CSV
//   con encabezado e ID como primera columna.
// -----------------------------------------------------------------------------

#ifndef COORDINATOR_H
#define COORDINATOR_H

#include "common.h"

void coordinator_run(const names_t *nn, int total, const char *csv_path);

#endif // COORDINATOR_H
