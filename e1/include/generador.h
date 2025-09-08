#ifndef GENERADOR_H
#define GENERADOR_H

#include "common.h"
#include "ipc.h"
#include "randrec.h"
#include <signal.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

// Prototipo expuesto
void generator_loop(int idx_generador);

#endif /* GENERADOR_H */
