#include "../include/randrec.h"

static const char *PRODUCTOS[] = {
    "CPU AMD Ryzen 5 5600X",
    "CPU Intel Core i5-12400F",
    "Motherboard B550M",
    "Motherboard Z690",
    "Memoria RAM DDR4 8GB 3200",
    "Memoria RAM DDR4 16GB 3200",
    "Memoria RAM DDR5 16GB 5200",
    "SSD NVMe 500GB",
    "SSD NVMe 1TB",
    "SSD SATA 480GB",
    "HDD 2TB 7200rpm",
    "Placa de video RTX 3060",
    "Placa de video RTX 4060",
    "Placa de video RX 6600",
    "Fuente 650W 80+ Bronze",
    "Fuente 750W 80+ Gold",
    "Gabinete ATX con vidrio templado",
    "Cooler CPU torre 120mm",
    "Kit 3 coolers ARGB 120mm",
    "Monitor 24\" 1080p 144Hz",
    "Monitor 27\" 1440p 165Hz",
    "Teclado mecánico TKL",
    "Mouse gamer 16000 DPI",
    "Combo teclado y mouse inalámbricos",
    "Auriculares gamer 7.1",
    "Micrófono condensador USB",
    "Placa madre H610M",
    "Placa madre B760",
    "Router WiFi 6 AX1800",
    "Adaptador WiFi USB",
    "Webcam 1080p",
    "Cámara IP 2K",
    "Capturadora HDMI USB",
    "Hub USB 3.0 7 puertos",
    "Cargador USB-C 65W",
    "UPS 1200VA",
    "Dock NVMe USB-C",
    "Lector tarjetas SD",
    "Pad mouse XL",
    "Soporte monitor articulado",
    "Silla gamer reclinable",
    "Notebook 15\" i5 8GB 512GB",
    "Mini PC N100 16GB 512GB",
    "Raspberry Pi 4 8GB",
    "Disipador M.2",
    "Cable HDMI 2.1 2m",
    "Cable DisplayPort 1.4 2m",
    "Pasta térmica 5g",
    "Switch gigabit 8 puertos",
    "NAS 2 bahías",
    "Enclosure 2.5\" USB 3.0"};
static const size_t CANT_PRODUCTOS = sizeof(PRODUCTOS) / sizeof(PRODUCTOS[0]);

void semilla_randrec(void)
{
    static int semilla = 0;
    if (!semilla)
    {
        srand((unsigned)time(NULL) ^ (unsigned)getpid());
        semilla = 1;
    }
}

static inline float rand_rango_float(float limInf, float limSup)
{
    float n = (float)rand() / (float)RAND_MAX;
    return limInf + n * (limSup - limInf);
}

void generar_randrec(registro_t *r, uint32_t id, int generar_idx)
{
    semilla_randrec();
    memset(r, 0, sizeof(*r));
    char buffer_aux[64];
    const time_t ahora = time(NULL);

    r->id = id;
    r->generador = generar_idx;
    r->pid = getpid();

    // Nombre del producto
    const char *p = PRODUCTOS[rand() % CANT_PRODUCTOS];
    strncpy(r->nombre, p, NOMBRE_MAXLEN - 1);
    r->nombre[NOMBRE_MAXLEN - 1] = '\0';

    // Precio [1000.00, 100000.00] y Stock [0..100]
    r->precio = rand_rango_float(1000.0f, 100000.0f);
    r->stock = (uint32_t)(rand() % 101);
    strftime(buffer_aux, sizeof(buffer_aux), "%Y-%m-%d %H:%M:%S", localtime(&ahora));
    strcpy(r->timestamp,buffer_aux);
    r->borrado = 0;
}
