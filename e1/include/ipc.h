#ifndef IPC_H
#define IPC_H

#include "common.h"

// punteros globales a segmentos compartidos
extern ring_t*      g_ring;
extern ids_state_t* g_ids;

// semáforos globales
extern sem_t* sem_empty;
extern sem_t* sem_full;
extern sem_t* sem_mutex;
extern sem_t* sem_ids;

// init / close
int  ipc_open_all(int create, uint32_t total_ids);
void ipc_close_all(int unlink_now);

// ring ops
void ring_push(const record_t* r);
int  ring_pop(record_t* out);

// ids ops (bloque de 10 máximo)
int  ids_take_block(uint32_t* base, uint32_t* count);

#endif
