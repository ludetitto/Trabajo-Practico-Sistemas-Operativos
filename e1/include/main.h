#ifndef MAIN_H
#define MAIN_H

// Dependencias globales necesarias para main.c
#include "common.h"
#include "ipc.h"
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // Implementadas en otros módulos:
    void generator_loop(int idx_generador);
    void coordinator_run(int total, const char *csvpath);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_H */
