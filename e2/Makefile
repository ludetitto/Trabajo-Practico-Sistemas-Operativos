CC=gcc
CFLAGS=-O2 -Wall -Wextra -std=c11 -Iinclude
LDFLAGS=-pthread
OBJDIR=obj
BINDIR=bin

SRV_SRC=src/server.c src/csvdb.c src/proto.c
CLN_SRC=src/client.c
SRV_OBJ=$(patsubst src/%.c,$(OBJDIR)/%.o,$(SRV_SRC))
CLN_OBJ=$(patsubst src/%.c,$(OBJDIR)/%.o,$(CLN_SRC))

.PHONY: all clean run demo interactive client

all: $(BINDIR)/server $(BINDIR)/client

$(OBJDIR)/%.o: src/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BINDIR)/server: $(SRV_OBJ) | $(BINDIR)
	$(CC) $^ -o $@ $(LDFLAGS)

$(BINDIR)/client: $(CLN_OBJ) | $(BINDIR)
	$(CC) $^ -o $@ $(LDFLAGS)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

clean:
	rm -rf $(OBJDIR) $(BINDIR) db.csv

# =====================================================================
# Targets de ejecución
# =====================================================================

# run: demo mínima usando CSV local (e2/db.csv)
run: all
	@echo ">>> CSV local (e2/db.csv)"
	@if [ ! -f db.csv ]; then \
	  echo "id,generador,pid,Nombre" > db.csv; \
	  echo "1,0,123,Juan" >> db.csv; \
	  echo "2,0,456,Ana"  >> db.csv; \
	fi
	@./bin/server -H 127.0.0.1 -p 5000 -n 2 -m 8 -f db.csv & \
	SERVER_PID=$$!; \
	sleep 1; \
	( echo "PING"; \
	  echo "GET 1"; \
	  echo "BEGIN"; \
	  echo "ADD nombre=Zeus generador=9 pid=777"; \
	  echo "UPDATE id=1 nombre=Pepe"; \
	  echo "COMMIT"; \
	  echo "QUIT" ) | ./bin/client -h 127.0.0.1 -p 5000; \
	kill $$SERVER_PID || true

# demo: igual que run (alias); podés personalizarlo aparte si querés
demo: run

# interactive: deja el servidor corriendo y la consola libre
# (Luego abrí otra terminal y ejecutá ./bin/client -h 127.0.0.1 -p 5000)
interactive: all
	@echo ">>> Servidor escuchando en 127.0.0.1:5000 (Ctrl+C para salir)"
	@if [ ! -f db.csv ]; then \
	  echo "id,generador,pid,Nombre" > db.csv; \
	  echo "1,0,123,Juan" >> db.csv; \
	  echo "2,0,456,Ana"  >> db.csv; \
	fi
	@./bin/server -H 127.0.0.1 -p 5000 -n 4 -m 16 -f db.csv

# client: abre un cliente interactivo (requiere server corriendo)
client: $(BINDIR)/client
	@./bin/client -h 127.0.0.1 -p 5000


reset-db:
	@echo "id,generador,pid,Nombre" > db.csv
	@echo "1,0,123,Juan"           >> db.csv
	@echo "2,0,456,Ana"            >> db.csv
	@echo ">>> db.csv reiniciado"