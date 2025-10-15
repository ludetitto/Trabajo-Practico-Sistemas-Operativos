#ifndef COORDINADOR_H
#define COORDINADOR_H

// Dependencias del coordinador
#include "common.h" // tipos básicos, matar(), registro_t, etc.
#include "ipc.h"    // pop_timeout(), ipc_restantes()
#include "csv.h"    // abrir_csv(), escribir_csv(), cerrar_csv()
#include <locale.h> // setlocale()
#include <signal.h> // señales y sigaction

#ifdef __cplusplus
extern "C" {
#endif

/* Ejecuta el rol de coordinador (consumidor):
   - Abre/crea el CSV (con encabezado desde csv.c si corresponde).
   - Consume EXACTAMENTE 'total' registros desde la cola (orden de arribo).
   - Loguea cada escritura y reacciona a señales para terminar ordenadamente.
   - Tolerancia a fallos: detecta muerte de generadores (SIGCHLD), marca alive[] = 0,
     y ajusta el turno RR para que el sistema continúe hasta completar 'total'.
*/
void coordinator_run(int total, const char *csvpath);

#ifdef __cplusplus
}
#endif

#endif /* COORDINADOR_H */


