CC=gcc
CFLAGS = -O2 -Wall -Wextra -std=c11 -Iinclude -D_GNU_SOURCE -D_DEFAULT_SOURCE
LDFLAGS = -pthread
OBJDIR=obj
BINDIR=bin

SRV_SRC=src/server.c src/csvdb.c src/proto.c
CLN_SRC=src/client.c
SRV_OBJ=$(patsubst src/%.c,$(OBJDIR)/%.o,$(SRV_SRC))
CLN_OBJ=$(patsubst src/%.c,$(OBJDIR)/%.o,$(CLN_SRC))
LOG_DIR=logs

all: $(BINDIR)/server $(BINDIR)/client

$(OBJDIR)/%.o: src/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BINDIR)/server: $(SRV_OBJ) | $(BINDIR)
	$(CC) $^ -o $@ $(LDFLAGS)

$(BINDIR)/client: $(CLN_OBJ) | $(BINDIR)
	$(CC) $^ -o $@ $(LDFLAGS)

$(OBJDIR) $(BINDIR):
	mkdir -p $@

$(LOG_DIR):
	mkdir -p $@

clean:
	rm -rf $(OBJDIR) $(BINDIR) $(LOG_DIR) test*.csv

# Limpiar archivos de test
clean-tests:
	rm -rf $(LOG_DIR) test*.csv

# Función para matar el servidor anterior si existe
define kill_server
	@-kill `cat .srvpid 2>/dev/null` 2>/dev/null || true
	@rm -f .srvpid
endef

# Función para iniciar el servidor
define start_server
	@echo "Iniciando servidor en puerto $(1)..." | tee -a $(LOG_DIR)/test$(2).log
	@./$(BINDIR)/server -H 127.0.0.1 -p $(1) -n 4 -m 16 -f test$(2).csv > $(LOG_DIR)/server$(2).log 2>&1 & echo $$! > .srvpid
	@sleep 2
endef

# Función para crear CSV inicial
define create_csv
	@echo "Creando archivo CSV inicial..." | tee -a $(LOG_DIR)/test$(1).log
	@echo "id,generador,pid,nombre,precio,stock" > test$(1).csv
	@echo "1,1,100,Producto1,10.5,50" >> test$(1).csv
	@echo "2,1,101,Producto2,20.0,30" >> test$(1).csv
endef

# Test 1: Inicialización del Servidor (modo guiado)
test1: all | $(LOG_DIR)
	@echo "=== Test 1: Inicialización del Servidor (modo guiado) ===" | tee $(LOG_DIR)/test1.log
	$(call create_csv,1)
	@echo ""; echo "Servidor por iniciar en puerto 5001 usando test1.csv"; \
	 echo "Pasos:"; \
	 echo "  1) Abrí otra terminal y ubicáte en e2/"; \
	 echo "  2) Ejecutá el cliente interactivo:"; \
	 echo "     $$ ./bin/client -h 127.0.0.1 -p 5001"; \
	 echo "  3) En esa terminal escribí, una por línea (esperado a la derecha):"; \
	 echo "     PING   # => OK"; \
	 echo "     QUIT   # => BYE"; \
	 echo "  4) Volvé a esta terminal y presioná Ctrl+C para detener el server"; echo "";
	@./$(BINDIR)/server -H 127.0.0.1 -p 5001 -n 4 -m 16 -f test1.csv

# Test 2: Conexiones Múltiples (modo guiado)
test2: all | $(LOG_DIR)
	@echo "=== Test 2: Conexiones Múltiples (modo guiado) ===" | tee $(LOG_DIR)/test2.log
	$(call create_csv,2)
	@echo ""; echo "Servidor por iniciar en puerto 5002 usando test2.csv"; \
	 echo "Pasos:"; \
	 echo "  1) Abrí otra(s) terminal(es) y ubicáte en e2/"; \
	 echo "  2) En cada una, ejecutá el cliente:"; \
	 echo "     $$ ./bin/client -h 127.0.0.1 -p 5002"; \
	 echo "  3) Escribí en cada cliente:"; \
	 echo "     PING   # => OK"; \
	 echo "     QUIT   # => BYE"; \
	 echo "  4) Repetí para probar múltiples conexiones"; \
	 echo "  5) Volvé a esta terminal y presioná Ctrl+C para detener el server"; echo "";
	@./$(BINDIR)/server -H 127.0.0.1 -p 5002 -n 4 -m 16 -f test2.csv

# Test 3: Transacciones y Locks (modo guiado)
test3: all | $(LOG_DIR)
	@echo "=== Test 3: Transacciones y Locks (modo guiado) ===" | tee $(LOG_DIR)/test3.log
	$(call create_csv,3)
	@echo ""; echo "Servidor por iniciar en puerto 5003 usando test3.csv"; \
	 echo "Pasos (dos clientes):"; \
	 echo "  1) Terminal A: ubicáte en e2/ y ejecutá:"; \
	 echo "     $$ ./bin/client -h 127.0.0.1 -p 5003"; \
	 echo "     Luego escribí:"; \
	 echo "       PING                                # => OK"; \
	 echo "       BEGIN                               # => OK"; \
	 echo "       ADD nombre=Test1 precio=10.5 stock=5 # => OK"; \
	 echo "       COMMIT                              # => OK"; \
	 echo "       QUIT                                # => BYE"; \
	 echo "  2) Terminal B: (cuando A haya hecho COMMIT) ejecutá:"; \
	 echo "     $$ ./bin/client -h 127.0.0.1 -p 5003"; \
	 echo "     Luego escribí:"; \
	 echo "       PING                                # => OK"; \
	 echo "       BEGIN                               # => OK"; \
	 echo "       ADD nombre=Test2 precio=12.5 stock=7 # => OK"; \
	 echo "       COMMIT                              # => OK"; \
	 echo "       QUIT                                # => BYE"; \
	 echo "  3) Volvé a esta terminal y presioná Ctrl+C para detener el server"; echo "";
	@./$(BINDIR)/server -H 127.0.0.1 -p 5003 -n 4 -m 16 -f test3.csv

# Test 4: Manejo de Snapshots (modo guiado)
test4: all | $(LOG_DIR)
	@echo "=== Test 4: Manejo de Snapshots (modo guiado) ===" | tee $(LOG_DIR)/test4.log
	$(call create_csv,4)
	@echo ""; echo "Servidor por iniciar en puerto 5004 usando test4.csv"; \
	 echo "Pasos:"; \
	 echo "  1) Abrí otra terminal, ubicáte en e2/ y ejecutá:"; \
	 echo "     $$ ./bin/client -h 127.0.0.1 -p 5004"; \
	 echo "  2) Escribí, una por línea:"; \
	 echo "     PING                               # => OK"; \
	 echo "     BEGIN                              # => OK"; \
	 echo "     ADD nombre=Test3 precio=33.0 stock=3 # => OK"; \
	 echo "     ROLLBACK                           # => OK"; \
	 echo "     QUIT                               # => BYE"; \
	 echo "  3) (Opcional) Si probás COMMIT después de ROLLBACK, esperá: ERR NOT_OWNER_OR_NO_TX"; \
	 echo "  4) Volvé a esta terminal y presioná Ctrl+C para detener el server"; echo "";
	@./$(BINDIR)/server -H 127.0.0.1 -p 5004 -n 4 -m 16 -f test4.csv

# Test 5: Protocolo de Comunicación (modo guiado)
test5: all | $(LOG_DIR)
	@echo "=== Test 5: Protocolo de Comunicación (modo guiado) ===" | tee $(LOG_DIR)/test5.log
	$(call create_csv,5)
	@echo ""; echo "Servidor por iniciar en puerto 5005 usando test5.csv"; \
	 echo "Pasos:"; \
	 echo "  1) Abrí otra terminal, ubicáte en e2/ y ejecutá:"; \
	 echo "     $$ ./bin/client -h 127.0.0.1 -p 5005"; \
	 echo "  2) Escribí, una por línea (y lo esperado):"; \
	 echo "     INVALID_COMMAND                         # => ERR CMD"; \
	 echo "     PING                                    # => OK"; \
	 echo "     BEGIN                                   # => OK"; \
	 echo "     ADD nombre=Test4                        # => ERR ARG"; \
	 echo "     ADD nombre=Test5 precio=50.0 stock=10   # => OK"; \
	 echo "     COMMIT                                  # => OK"; \
	 echo "     QUIT                                    # => BYE"; \
	 echo "  3) Volvé a esta terminal y presioná Ctrl+C para detener el server"; echo "";
	@./$(BINDIR)/server -H 127.0.0.1 -p 5005 -n 4 -m 16 -f test5.csv

# Test 6: Recuperación ante Fallos (modo guiado)
test6: all | $(LOG_DIR)
	@echo "=== Test 6: Recuperación ante Fallos (modo guiado) ===" | tee $(LOG_DIR)/test6.log
	$(call create_csv,6)
	@echo ""; echo "Servidor por iniciar en puerto 5006 usando test6.csv"; \
	 echo "Pasos:"; \
	 echo "  1) Abrí otra terminal, ubicáte en e2/ y ejecutá:"; \
	 echo "     $$ ./bin/client -h 127.0.0.1 -p 5006"; \
	 echo "  2) Escribí (dejá la TX abierta, no pongas QUIT):"; \
	 echo "     BEGIN                                # => OK"; \
	 echo "     ADD nombre=Test6 precio=60.0 stock=5 # => OK"; \
	 echo "  3) Volvé a esta terminal y presioná Ctrl+C para matar el server"; \
	 echo "  4) (Verificación) Reiniciá el test y corré un cliente: 'FIND Test6' NO debe devolver RESULT"; echo "";
	@./$(BINDIR)/server -H 127.0.0.1 -p 5006 -n 4 -m 16 -f test6.csv

# Test 7: Concurrencia y Performance (modo guiado)
test7: all | $(LOG_DIR)
	@echo "=== Test 7: Concurrencia y Performance (modo guiado) ===" | tee $(LOG_DIR)/test7.log
	$(call create_csv,7)
	@echo ""; echo "Servidor por iniciar en puerto 5007 usando test7.csv"; \
	 echo "Pasos:"; \
	 echo "  1) Abrí varias terminales, ubicáte en e2/ y en cada una ejecutá:"; \
	 echo "     $$ ./bin/client -h 127.0.0.1 -p 5007"; \
	 echo "  2) En cada cliente escribí:"; \
	 echo "     BEGIN                                # => OK (o ERR TX_ACTIVE si otro tiene TX)"; \
	 echo "     ADD nombre=TestX precio=70.0 stock=9 # => OK"; \
	 echo "     COMMIT                               # => OK"; \
	 echo "     QUIT                                 # => BYE"; \
	 echo "  (Si otro cliente tiene una TX activa, BEGIN devolverá ERR TX_ACTIVE; reintentar luego)"; \
	 echo "  3) Volvé a esta terminal y presioná Ctrl+C para detener el server"; echo "";
	@./$(BINDIR)/server -H 127.0.0.1 -p 5007 -n 4 -m 16 -f test7.csv

# Ejecutar todos los tests
test: all | $(LOG_DIR)
	@echo "Ejecutando todos los tests..."
	@make test1
	@make test2
	@make test3
	@make test4
	@make test5
	@make test6
	@make test7
	@echo "Tests completados. Ver resultados en carpeta logs/"

# Demo: usa el CSV del E1; si no existe, crea uno chiquito
demo_queue: all
	@if [ ! -f ../db.csv ]; then \
	  echo "id,generador,pid,Nombre" > ../db.csv; \
	  echo "1,1,758,Monitor 24" 1080p 144Hz,19708.73,35" >> ../db.csv; \
	  echo "11,0,757,Lector tarjetas SD,28274.66,58"  >> ../db.csv; \
	  echo "21,2,759,Mouse gamer 16000 DPI,60620.63,78" >> ../db.csv; \
	fi
	@echo ">>> Lanzando server (CSV=../e1/db.csv) en 127.0.0.1:5000"
	@./bin/server -H 127.0.0.1 -p 5000 -n 4 -m 16 -f ../db.csv & echo $$! > .srvpid
	@sleep 1
	@echo ">>> Script de cliente (cola por evento + tx):"
	@( \
	  echo "PING"; \
	  echo "SHOWQ evento=Lollapalooza"; \
	  echo "ADD nombre=Zeus generador=9 pid=888 evento=Lollapalooza"; \
	  echo "BEGIN"; \
	  echo "COMMIT"; \
	  echo "QUIT" \
	) | ./bin/client -h 127.0.0.1 -p 5000
	@kill $$(cat .srvpid) >/dev/null 2>&1 || true; rm -f .srvpid

# Ejecuta solo el servidor usando el CSV del E1
server: all
	@if [ ! -f ../db.csv ]; then \
	  echo "Falta ../db.csv (generalo con el Ejercicio 1 o crea uno)"; exit 1; \
	fi
	@echo ">>> Server en 127.0.0.1:5000 usando ../db.csv"
	@./bin/server -H 127.0.0.1 -p 5000 -n 4 -m 16 -f ../db.csv

# Cliente interactivo
client: all
	@./bin/client -h 127.0.0.1 -p 5000

.PHONY: all clean demo_queue server client

