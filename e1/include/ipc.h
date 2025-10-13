#ifndef IPC_H
#define IPC_H

// ===== Includes de dependencias (usadas por el header y la impl.) =====
#include "common.h" // registro_t, cola_t, ids_t, constantes, COLA_CAP, MAX_PRODS

// Estos headers estándar son necesarios para los tipos/funciones que expone IPC:
#include <string.h> // memset
#include <time.h>   // clock_gettime, timespec
#include <errno.h>  // errno, ETIMEDOUT

#ifdef __cplusplus
extern "C"
{
#endif

    // ===== Punteros globales a segmentos compartidos =====
    // - 'cola'      : ring buffer de registros compartido entre productores/consumidor.
    // - 'ids_estado': estado global de asignación de IDs y metadatos de RR.
    extern cola_t *cola;
    extern ids_t *ids_estado;

    // ===== Semáforos globales =====
    // - empty: espacios libres en el ring
    // - full : elementos ocupados en el ring
    // - mutex: exclusión mutua del ring
    // - ids  : exclusión mutua del estado de IDs / RR
    extern sem_t *sem_empty;
    extern sem_t *sem_full;
    extern sem_t *sem_mutex;
    extern sem_t *sem_ids;

    // ===== Apertura / cierre de IPCs =====
    // Crea/abre todos los recursos compartidos (SHM + semáforos).
    //   crear=1 => crea/inicializa; crear=0 => solo abre.
    //   total_ids: cantidad total de IDs a asignar (solo se usa al crear).
    int ipc_abrir_todos(int crear, uint32_t total_ids);

    // Cierra y opcionalmente destruye (unlink) todos los IPCs.
    //   borrar_ahora=1 => hace shm_unlink/sem_unlink; 0 => solo close/munmap.
    void ipc_cerrar_todos(int borrar_ahora);

    // ===== Primitivas de cola (productor/consumidor) =====
    // push: inserta un registro (bloquea si el ring está lleno).
    void push(const registro_t *r);

    // pop: extrae un registro (bloquea hasta que haya datos).
    int pop(registro_t *r);

    // pop_timeout: extrae con timeout en milisegundos.
    //   return: 0=ok, 1=timeout, -1=error.
    int pop_timeout(registro_t *r, int timeout_ms);

    int push_interruptible(const registro_t *r, int timeout_ms);

    // ===== Operaciones sobre IDs (RR estricto y estado de hijos) =====
    // Asigna un bloque de IDs (máx 10) en estricto round-robin, saltando hijos muertos.
    //   idx: índice lógico del generador [0..nprods-1].
    //   base/cant: salida con primer ID y cantidad asignada.
    //   return: 0=asignó, 1=no quedan IDs.
    int pedir_bloque_ids_rr(int idx, uint32_t *base, uint32_t *cant);

    // Publica la tabla de hijos (pids) y los marca vivos; inicializa turno=0.
    // Debe llamarse una vez desde el padre luego del fork.
    void ipc_set_children(int nprods, const pid_t *pids);

    // Marca como muerto al hijo con ese PID (invocar desde handler de SIGCHLD).
    void ipc_mark_dead(pid_t pid);

    // Devuelve cuántos IDs faltan por asignar (lectura atómica bajo sem_ids).
    uint32_t ipc_restantes(void);
    // devuelve la cantidad de generadores marcados como vivos
    int ipc_prods_vivos(void);
    // número total de generadores publicados por el padre (ipc_set_children)
    int ipc_nprods(void);
    // retorna 1 si algun generador murió antes de consumirse todos los IDs
    int ipc_hubo_muerte_prematura(void);

#ifdef __cplusplus
}
#endif

#endif /* IPC_H */
