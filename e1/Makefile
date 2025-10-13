# === Makefile: un solo binario (tp_e1) ===

CC       = gcc
CFLAGS   = -O2 -Wall -Wextra -std=c11 -Iinclude
# En algunas distros, shm_open requiere -lrt. Si falla el link, descomentar:
# LDLIBS   = -pthread -lrt
LDLIBS   = -pthread

SRC_DIR  = src
OBJ_DIR  = obj
BIN_DIR  = bin
LOG_DIR  = logs

# Agregá/quita fuentes según tu repo
SRCS = \
  $(SRC_DIR)/main.c \
  $(SRC_DIR)/coordinador.c \
  $(SRC_DIR)/generador.c \
  $(SRC_DIR)/ipc.c \
  $(SRC_DIR)/csv.c \
  $(SRC_DIR)/randrec.c

OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
BIN  = $(BIN_DIR)/main

.PHONY: all clean run demo validate

all: $(BIN)

$(BIN): $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDLIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR) $(BIN_DIR) $(LOG_DIR):
	mkdir -p $@
#elimina solo los archivos generados (binarios y objetos)
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR) $(LOG_DIR)
# elimina TODOS los archivos generados (binarios, objetos, CSVs)
cleanAll:
	rm -rf $(OBJ_DIR) $(BIN_DIR) $(LOG_DIR) *.csv test*.csv
#elimina TODOS LOS ARCHIVOS .csv del directorio actual
cleanAll-csv:
	rm -f *.csv
#elimina un csv especifico, se ejecuta así-> make clean-csv FILE=<nombreArchivo.csv> !!sin especacios entre el igual!!
clean-csv:
	rm -f $(FILE) 

# Limpiar logs y archivos de prueba
cleanAll-tests:
	rm -rf $(LOG_DIR) test*.csv

# === Casos de prueba ===
.PHONY: test test1 test2 test3 test4 test5 test6 test7

# Ejecutar todos los casos de prueba
test: $(BIN) | $(LOG_DIR)
	@echo "Ejecutando todos los casos de prueba..."
	@make test1
	@make test2
	@make test3
	@make test4
	@make test5
	@make test6
	@make test7
	@echo "Todos los tests completados. Ver resultados en la carpeta logs/"

# Test 1: Inicialización de IPCs
test1: $(BIN) | $(LOG_DIR)
	@echo "=== Test 1: Inicialización de IPCs ===" | tee $(LOG_DIR)/test1.log
	@echo "Probando con 1 productor..." | tee -a $(LOG_DIR)/test1.log
	@./$(BIN) -n 1 -t 100 -o test1_1.csv 2>&1 | tee -a $(LOG_DIR)/test1.log
	@echo "Probando con 5 productores..." | tee -a $(LOG_DIR)/test1.log
	@./$(BIN) -n 5 -t 100 -o test1_5.csv 2>&1 | tee -a $(LOG_DIR)/test1.log
	@echo "Verificando IPCs..." | tee -a $(LOG_DIR)/test1.log
	@ipcs -a 2>&1 | tee -a $(LOG_DIR)/test1.log

# Test 2: Sincronización Productor-Consumidor
test2: $(BIN) | $(LOG_DIR)
	@echo "=== Test 2: Sincronización Productor-Consumidor ===" | tee $(LOG_DIR)/test2.log
	@./$(BIN) -n 3 -t 100 -o test2.csv 2>&1 | tee -a $(LOG_DIR)/test2.log
	@echo "Verificando IDs únicos..." | tee -a $(LOG_DIR)/test2.log
	@awk -f scripts/verificar_ids.awk test2.csv 2>&1 | tee -a $(LOG_DIR)/test2.log

# Test 3: Manejo de Buffer Lleno
test3: $(BIN) | $(LOG_DIR)
	@echo "=== Test 3: Manejo de Buffer Lleno ===" | tee $(LOG_DIR)/test3.log
	@echo "Ejecutando con productores rápidos y consumidor lento..." | tee -a $(LOG_DIR)/test3.log
	@./$(BIN) -n 4 -t 200 -o test3.csv 2>&1 | tee -a $(LOG_DIR)/test3.log

# Test 4: Manejo de Buffer Vacío
test4: $(BIN) | $(LOG_DIR)
	@echo "=== Test 4: Manejo de Buffer Vacío ===" | tee $(LOG_DIR)/test4.log
	@echo "Ejecutando con productores lentos..." | tee -a $(LOG_DIR)/test4.log
	@./$(BIN) -n 2 -t 50 -o test4.csv 2>&1 | tee -a $(LOG_DIR)/test4.log

# Test 5: Exclusión Mutua en Asignación de IDs
test5: $(BIN) | $(LOG_DIR)
	@echo "=== Test 5: Exclusión Mutua en Asignación de IDs ===" | tee $(LOG_DIR)/test5.log
	@./$(BIN) -n 10 -t 500 -o test5.csv 2>&1 | tee -a $(LOG_DIR)/test5.log
	@echo "Verificando IDs únicos..." | tee -a $(LOG_DIR)/test5.log
	@awk -f scripts/verificar_ids.awk test5.csv 2>&1 | tee -a $(LOG_DIR)/test5.log

# Test 6: Manejo de Señales
test6: $(BIN) | $(LOG_DIR)
	@echo "=== Test 6: Manejo de Señales ===" | tee $(LOG_DIR)/test6.log
	@echo "Iniciando programa..." | tee -a $(LOG_DIR)/test6.log
	@./$(BIN) -n 5 -t 1000 -o test6.csv & echo $$! > pid.txt
	@sleep 2
	@echo "Enviando SIGTERM..." | tee -a $(LOG_DIR)/test6.log
	@kill -TERM $$(cat pid.txt) 2>&1 | tee -a $(LOG_DIR)/test6.log
	@rm -f pid.txt
	@sleep 1
	@echo "Verificando recursos liberados:" | tee -a $(LOG_DIR)/test6.log
	@ipcs -a 2>&1 | tee -a $(LOG_DIR)/test6.log

# Test 7: Recuperación ante Fallos
test7: $(BIN) | $(LOG_DIR)
	@echo "=== Test 7: Recuperación ante Fallos ===" | tee $(LOG_DIR)/test7.log
	@echo "Iniciando programa..." | tee -a $(LOG_DIR)/test7.log
	@./$(BIN) -n 3 -t 200 -o test7.csv 2>&1 | tee -a $(LOG_DIR)/test7.log

# Ejecución rápida pasando parámetros arbitrarios
# Uso: make run ARGS="-n 3 -t 90 -o productos.csv"
run: all
	$(BIN) -n 4 -t 100 -o ../db.csv

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
