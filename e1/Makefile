# Makefile (reemplazalo completo)

CC       = gcc
CFLAGS   = -O2 -Wall -Wextra -std=c11 -Iinclude # nivel de optimización, warnings, estandar C, include
LDFLAGS  = -pthread

SRC_DIR  = src
OBJ_DIR  = obj
BIN_DIR  = bin

# Objetos por binario (no hay más cola.o)
COORD_SRCS = coordinador.c ipc.c csv.c
GEN_SRCS   = generador.c ipc.c randrec.c

COORD_OBJS = $(addprefix $(OBJ_DIR)/,$(COORD_SRCS:.c=.o))
GEN_OBJS   = $(addprefix $(OBJ_DIR)/,$(GEN_SRCS:.c=.o))

BINS = $(BIN_DIR)/coordinador $(BIN_DIR)/generador

.PHONY: all clean run

all: $(BINS)

$(BIN_DIR)/coordinador: $(COORD_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BIN_DIR)/generador: $(GEN_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR) $(BIN_DIR):
	mkdir -p $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# Ejemplo de ejecución: 3 generadores, 90 registros totales
# (coordinador elimina IPCs al terminar)
run: all
	@set -e; \
	out="productos.csv"; \
	N=3; TOTAL=90; PORG=$$((TOTAL/N)); \
	echo "[+] Ejecutando coordinador -> $$out"; \
	$(BIN_DIR)/coordinador -n $$N -t $$TOTAL -f $$out & \
	PID_COORD=$$!; \
	for i in `seq 0 $$((N-1))`; do \
		echo "[+] Generador $$i con $$PORG registros"; \
		$(BIN_DIR)/generador -q $$PORG -g $$i & \
	done; \
	wait $$PID_COORD; \
	echo "[+] Listo. CSV generado: $$out"
