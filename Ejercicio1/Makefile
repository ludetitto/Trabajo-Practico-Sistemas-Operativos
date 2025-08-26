# Makefile – Ejercicio 1 (mejorado)

CC       ?= gcc
TARGET    = ej1
SRC       = ej1.c

CFLAGS   := -O2 -Wall -Wextra -std=c11
LDFLAGS  := -pthread
# Si tu distro requiere librt para clock_gettime/sem_timedwait, descomentá:
# LDFLAGS  += -lrt

.PHONY: all run demo validate clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(SRC) -o $(TARGET) $(CFLAGS) $(LDFLAGS)

# Ejecución de ejemplo (3 generadores, 100 registros, sin delay)
run: all
	./$(TARGET) -n 3 -t 100 -f registros.csv -d 0

# Demo completa (compila → corre → valida) usando scripts/
demo: all
	./scripts/run_demo.sh

# Validador AWK sobre registros.csv (requiere scripts/awk_idcheck.awk)
validate:
	awk -f scripts/awk_idcheck.awk registros.csv

clean:
	rm -f $(TARGET) registros.csv
