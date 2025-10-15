#ifndef IPC_H
#define IPC_H

#include "common.h"
#include <string.h>
#include <time.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

// ===== Punteros globales a SHM =====
extern cola_t *cola;
extern ids_t  *ids_estado;

// ===== Semáforos globales =====
extern sem_t *sem_empty;
extern sem_t *sem_full;
extern sem_t *sem_mutex;
extern sem_t *sem_ids;

// ===== Apertura / cierre de IPCs =====
int  ipc_abrir_todos(int crear, uint32_t total_ids);
void ipc_cerrar_todos(int borrar_ahora);

// ===== Ring buffer =====
void push(const registro_t *r);
int  pop(registro_t *r);
int  pop_timeout(registro_t *r, int timeout_ms);

// Empuje con timeout (para reaccionar a señales): 0 ok, 1 timeout, -1 error
int  push_interruptible(const registro_t *r, int timeout_ms);

// ===== RR / IDs =====
int  pedir_bloque_ids_rr(int idx, uint32_t *base, uint32_t *cant);
void ipc_set_children(int nprods, const pid_t *pids);
void ipc_mark_dead(pid_t pid);             // devuelve faltantes del bloque
void ipc_avance_bloque(int idx);           // +1 al avance del bloque vigente

// Consultas
uint32_t ipc_restantes(void);
uint32_t ipc_pendientes_total(void);       // restantes + sum(devq)
int      ipc_prods_vivos(void);
int      ipc_nprods(void);
int      ipc_hubo_muerte_prematura(void);

#ifdef __cplusplus
}
#endif
#endif /* IPC_H */
