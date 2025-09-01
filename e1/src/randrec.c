#include "../include/randrec.h"

static const char* NOMBRES[] = {
    "Luis","Marta","Juan","Sofia","Pedro","Ana","Diego","Clara","Beta","Omega","Kappa","Alpha","Gamma","Delta","Lambda"
};
static const size_t CANT_NOMBRES = sizeof(NOMBRES)/sizeof(NOMBRES[0]);

void semilla_randrec(void) { // llamar una vez al inicio de cada proceso
    static int semilla = 0;
    if (!semilla) { 
        srand((unsigned)time(NULL) ^ (unsigned)getpid()); 
        semilla = 1; 
    } // semilla diferente por proceso
}

void generar_randrec(registro_t* r, uint32_t id, int generar_idx) { // genera un registro aleatorio
    semilla_randrec();
    memset(r, 0, sizeof(*r));
    r->id = id;
    r->generador = generar_idx;
    r->pid = getpid(); // PID del proceso que generó el registro
    strncpy(r->nombre, NOMBRES[rand() % CANT_NOMBRES], NOMBRE_MAXLEN-1);
}
