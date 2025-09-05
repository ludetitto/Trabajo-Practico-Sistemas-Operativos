#ifndef IPC_H
#define IPC_H

#include "common.h"

// punteros globales a segmentos compartidos
extern cola_t*      cola;
extern ids_t*       ids_estado;

// semáforos globales
extern sem_t* sem_empty;
extern sem_t* sem_full;
extern sem_t* sem_mutex;
extern sem_t* sem_ids;

// Abrir y cerrar todos los IPCs
int  ipc_abrir_todos(int crear, uint32_t total_ids);
void ipc_cerrar_todos(int borrar_ahora);

// Primitivas de cola
void push(const registro_t* r);
int  pop(registro_t* r);

// Operaciones sobre IDs
int  pedir_bloque_ids(uint32_t* base, uint32_t* cant);

#endif
