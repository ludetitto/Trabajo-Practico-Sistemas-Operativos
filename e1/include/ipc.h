#ifndef IPC_H
#define IPC_H

#include "common.h" // registro_t, cola_t, ids_t, constantes, COLA_CAP, MAX_PRODS

#ifdef __cplusplus
extern "C"
{
#endif

    // ===== Punteros globales a segmentos compartidos =====
    extern cola_t *cola;      // ring buffer de registros
    extern ids_t *ids_estado; // estado de IDs + RR

    // ===== Semáforos globales =====
    extern sem_t *sem_empty; // cuenta espacios vacíos en el ring
    extern sem_t *sem_full;  // cuenta elementos ocupados en el ring
    extern sem_t *sem_mutex; // mutex del ring
    extern sem_t *sem_ids;   // lock del estado de IDs

    // ===== Apertura / cierre de IPCs =====
    // crear=1 crea y dimensiona SHM + crea semáforos (padre); crear=0 sólo abre (hijos).
    int ipc_abrir_todos(int crear, uint32_t total_ids);
    void ipc_cerrar_todos(int borrar_ahora); // si borrar_ahora=1 hace shm_unlink/sem_unlink

    // ===== Primitivas de cola (productor/consumidor) =====
    void push(const registro_t *r);                 // bloqueante si ring lleno
    int pop(registro_t *r);                         // bloqueante hasta que haya datos
    int pop_timeout(registro_t *r, int timeout_ms); // 0=ok, 1=timeout, -1=error

    // ===== Operaciones sobre IDs (RR estricto y estado de hijos) =====

    // Asignación de bloques de IDs (máx 10) con RR estricto.
    // idx = índice lógico del generador [0..nprods-1].
    // Devuelve 0 si asignó, 1 si ya no quedan IDs.
    int pedir_bloque_ids_rr(int idx, uint32_t *base, uint32_t *cant);

    // Publica la tabla de hijos (pids) y los marca vivos; inicializa turno=0.
    // Debe llamarse una vez desde el padre después del fork.
    void ipc_set_children(int nprods, const pid_t *pids);

    // Marca como muerto al hijo con ese PID (handler de SIGCHLD del padre).
    void ipc_mark_dead(pid_t pid);

    // Devuelve cuántos IDs quedan por asignar (atómico bajo sem_ids).
    uint32_t ipc_restantes(void);

#ifdef __cplusplus
}
#endif

#endif /* IPC_H */
