# === Makefile: un solo binario (tp_e1) ===

CC       = gcc
CFLAGS   = -O2 -Wall -Wextra -std=c11 -Iinclude
# En algunas distros, shm_open requiere -lrt. Si falla el link, descomentar:
# LDLIBS   = -pthread -lrt
LDLIBS   = -pthread

SRC_DIR  = src
OBJ_DIR  = obj
BIN_DIR  = bin

# Agregá/quita fuentes según tu repo
SRCS = \
  $(SRC_DIR)/main.c \
  $(SRC_DIR)/coordinador.c \
  $(SRC_DIR)/generador.c \
  $(SRC_DIR)/ipc.c \
  $(SRC_DIR)/csv.c \
  $(SRC_DIR)/randrec.c

OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
BIN  = $(BIN_DIR)/tp_e1

.PHONY: all clean run demo validate

all: $(BIN)

$(BIN): $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDLIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR) $(BIN_DIR):
	mkdir -p $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# Ejecución rápida pasando parámetros arbitrarios
# Uso: make run ARGS="-n 3 -t 90 -o productos.csv"
run: all
	@echo "==> $(BIN) $(ARGS)"
	$(BIN) $(ARGS)

# Demo con valores por defecto
demo: all
	$(BIN) -n 3 -t 90 -o productos.csv

# Validación simple de IDs (sin huecos/duplicados) sobre productos.csv
validate:
	@awk -F, 'NR==1{if($$1!="id"){print "ERROR header"; exit 1} next} \
	{ id=$$1+0; if(id<=0){print "ERROR id inval",NR; exit 1} \
	  if(seen[id]++){print "ERROR duplicado",id; exit 1} \
	  if(NR==2) min=id; if(id<min) min=id; if(id>max) max=id; c++ } \
	END{ if(min!=1){print "ERROR min!=1:",min; exit 1} \
	     for(i=1;i<=max;i++) if(!seen[i]){print "ERROR falta id",i; exit 1} \
	     print "OK:",c,"regs, ids 1..",max }' productos.csv
