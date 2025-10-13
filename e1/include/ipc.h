#ifndef IPC_H
#define IPC_H

#include "common.h" // registro_t, cola_t, ids_t, COLA_CAP, MAX_PRODS, etc.
#include <semaphore.h>
#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#ifdef __cplusplus
extern "C"
{
#endif

    extern cola_t *cola;
    extern ids_t *ids_estado;

    extern sem_t *sem_empty;
    extern sem_t *sem_full;
    extern sem_t *sem_mutex;
    extern sem_t *sem_ids;

    int ipc_abrir_todos(int crear, uint32_t total_ids);
    void ipc_cerrar_todos(int borrar_ahora);

    // ring buffer
    void push(const registro_t *r);
    int pop(registro_t *r);
    int pop_timeout(registro_t *r, int timeout_ms);
    int push_interruptible(const registro_t *r, int timeout_ms);

    // IDs / RR
    int pedir_bloque_ids_rr(int idx, uint32_t *base, uint32_t *cant);
    void ipc_set_children(int nprods, const pid_t *pids);
    void ipc_mark_dead(pid_t pid);
    void ipc_avance_bloque(int idx);

    // Consultas
    uint32_t ipc_restantes(void);        // “nuevos” por asignar
    uint32_t ipc_pendientes_total(void); // NUEVO: restantes + devoluciones
    int ipc_prods_vivos(void);
    int ipc_nprods(void);
    int ipc_hubo_muerte_prematura(void);

#ifdef __cplusplus
}
#endif

#endif /* IPC_H */
