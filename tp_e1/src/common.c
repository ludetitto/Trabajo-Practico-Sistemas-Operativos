// src/common.c
// -----------------------------------------------------------------------------
// Propósito del archivo:
// - Implementación de utilidades comunes: manejo de errores, siembra RNG,
//   generación de campos de producto y construcción de nombres POSIX únicos.
// -----------------------------------------------------------------------------

#include "common.h"

void print_help_examples(const char *prog) {
    // Título
    printf("\n=== %s: ayuda con ejemplos ===\n\n", prog);

    // Recordatorio de uso básico
    printf("Uso basico:\n");
    printf("  %s -n <generadores> -t <total_registros> -o <salida.csv> [-b <capacidad_buffer>]\n\n", prog);

    // Parámetros explicados
    printf("Parametros:\n");
    printf("  -n : cantidad de generadores (procesos hijos productores)\n");
    printf("  -t : total de registros a generar (IDs correlativos 1..t)\n");
    printf("  -o : ruta del CSV de salida (ID primero)\n");
    printf("  -b : capacidad del buffer (cola circular). Por defecto: %d\n\n", DEFAULT_BUF_CAP);

// Lotes de prueba (recetas)
/*    printf("Lotes de prueba sugeridos:\n");
    printf("  # Pequeño (rapido para probar end-to-end)\n");
    printf("  %s -n 2 -t 50 -o productos_small.csv -b 16\n\n", prog);

    printf("  # Basico (tamanio medio, buffer razonable)\n");
    printf("  %s -n 4 -t 1000 -o productos.csv -b 128\n\n", prog);

    printf("  # Stress de concurrencia (muchos generadores) \n");
    printf("  %s -n 12 -t 5000 -o productos_stress.csv -b 256\n\n", prog);

    printf("  # Stress de backpressure (buffer chico para ver colas)\n");
    printf("  %s -n 6 -t 1000 -o productos_backpressure.csv -b 8\n\n", prog);

    printf("  # Un solo generador (depuracion sencilla)\n");
    printf("  %s -n 1 -t 100 -o productos_single.csv -b 32\n\n", prog);
*/
    // Tips de monitoreo (útiles en la defensa)
    printf("Monitoreo mientras corre:\n");
    printf("  ls -l /dev/shm | grep pc_     # ver SHM y semaforos POSIX de esta corrida\n");
    printf("  ps -ef | grep %s              # ver procesos\n", prog);
    printf("  vmstat 1                      # ver actividad del sistema\n\n");

    // Verificaciones rapidas
    printf("Verificaciones utiles post-ejecucion:\n");
    printf("  head -n 10 productos.csv\n");
    printf("  awk -F, 'NR>1{print $1}' productos.csv | sort -n | uniq -d   # IDs duplicados?\n");
    printf("  awk -F, 'NR>1{print $1}' productos.csv | sort -n | awk 'NR==1{p=$1;next}{if($1!=p+1)printf(\"Falta %%d\\n\",p+1);p=$1}'\n");
    printf("  # (Opcional) usar el check_ids.awk que te pase para validar segun la rubrica.\n\n");

    // Nota final
    printf("Notas:\n");
    printf("  - El coordinador consume EXACTAMENTE -t registros del buffer y escribe el CSV.\n");
    printf("  - Los generadores piden IDs de a bloques de 10; el ultimo bloque puede ser menor.\n");
    printf("  - El CSV tiene columnas: id,generador,pid,nombreProducto,precio,stock (ID primero).\n\n");
}


void die(const char *fmt, ...) {    // error fatal con mensaje formateado
    va_list ap; va_start(ap, fmt); // init varargs
    vfprintf(stderr, fmt, ap); // mensaje
    fputc('\n', stderr); // salto de línea
    va_end(ap); // end varargs
    exit(EXIT_FAILURE); // aborta
}

void perr(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void rand_seed(void) {
    // Semilla distinta por proceso (padre/hijos) para variedad de datos
    unsigned s = (unsigned)time(NULL) ^ (unsigned)getpid() ^ (unsigned)getppid();
    srand(s);
}

// Lista de nombres de productos de ejemplo (podés ampliar/editar)
static const char *PRODUCTS[] = {
    "Teclado Mecanico", "Mouse Gamer", "Auriculares BT", "Monitor 24\"",
    "SSD NVMe 1TB", "GPU RTX 4060", "Notebook 15\"", "Microfono USB",
    "Silla Gamer", "Pad Mouse XL", "Router WiFi 6", "Webcam 1080p",
    "Motherboard AM5", "Memoria 16GB", "Fuente 650W 80+"
};

// Completa los campos específicos del producto (no toca id/generador/pid)
void fill_random_product_fields(record_t *r) {
    // Nombre
    snprintf(r->nombreProducto, MAX_NAME, "%s",
        PRODUCTS[rand() % (int)(sizeof(PRODUCTS)/sizeof(PRODUCTS[0]))]);

    // Precio aleatorio con 2 decimales (en pesos, por ejemplo)
    // Generamos centavos enteros para evitar errores de redondeo
    int centavos = 99900 + rand() % 3000000;  // 999.00 .. 30999.99 (ajustable)
    r->precio = centavos / 100.0;

    // Stock en [0..500]
    r->stock = rand() % 501;
}

// Nombres únicos para recursos POSIX (evita colisiones entre corridas)
void gen_names(names_t *n) {
    int r = rand();
    snprintf(n->shm_name,       sizeof(n->shm_name),       "/pc_shm_%d_%d",   (int)getpid(), r);
    snprintf(n->sem_empty_name, sizeof(n->sem_empty_name), "/pc_empty_%d_%d", (int)getpid(), r);
    snprintf(n->sem_full_name,  sizeof(n->sem_full_name),  "/pc_full_%d_%d",  (int)getpid(), r);
    snprintf(n->sem_mutex_name, sizeof(n->sem_mutex_name), "/pc_mutex_%d_%d", (int)getpid(), r);
    snprintf(n->sem_id_name,    sizeof(n->sem_id_name),    "/pc_id_%d_%d",    (int)getpid(), r);
}
