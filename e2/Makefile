CC=gcc
CFLAGS = -O2 -Wall -Wextra -std=c11 -Iinclude -D_GNU_SOURCE -D_DEFAULT_SOURCE
LDFLAGS = -pthread
OBJDIR=obj
BINDIR=bin

SRV_SRC=src/server.c src/csvdb.c src/proto.c
CLN_SRC=src/client.c
SRV_OBJ=$(patsubst src/%.c,$(OBJDIR)/%.o,$(SRV_SRC))
CLN_OBJ=$(patsubst src/%.c,$(OBJDIR)/%.o,$(CLN_SRC))

all: $(BINDIR)/server $(BINDIR)/client

$(OBJDIR)/%.o: src/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BINDIR)/server: $(SRV_OBJ) | $(BINDIR)
	$(CC) $^ -o $@ $(LDFLAGS)

$(BINDIR)/client: $(CLN_OBJ) | $(BINDIR)
	$(CC) $^ -o $@ $(LDFLAGS)

$(OBJDIR) $(BINDIR):
	mkdir -p $@

clean:
	rm -rf $(OBJDIR) $(BINDIR)

# Demo: usa el CSV del E1; si no existe, crea uno chiquito
demo_queue: all
	@if [ ! -f ../e1/db.csv ]; then \
	  echo "id,generador,pid,Nombre" > ../e1/db.csv; \
	  echo "1,0,123,Juan" >> ../e1/db.csv; \
	  echo "2,0,456,Ana"  >> ../e1/db.csv; \
	  echo "3,1,777,Luis" >> ../e1/db.csv; \
	fi
	@echo ">>> Lanzando server (CSV=../e1/db.csv) en 127.0.0.1:5000"
	@./bin/server -H 127.0.0.1 -p 5000 -n 4 -m 16 -f ../e1/db.csv & echo $$! > .srvpid
	@sleep 1
	@echo ">>> Script de cliente (cola por evento + tx):"
	@( \
	  echo "PING"; \
	  echo "SHOWQ evento=Lollapalooza"; \
	  echo "ADD nombre=Zeus generador=9 pid=888 evento=Lollapalooza"; \
	  echo "SHOWQ evento=Lollapalooza"; \
	  echo "BEGIN"; \
	  echo "ATTEND evento=Lollapalooza"; \
	  echo "COMMIT"; \
	  echo "SHOWQ evento=Lollapalooza"; \
	  echo "QUIT" \
	) | ./bin/client -h 127.0.0.1 -p 5000
	@kill $$(cat .srvpid) >/dev/null 2>&1 || true; rm -f .srvpid

# Ejecuta solo el servidor usando el CSV del E1
run_server: all
	@if [ ! -f ../e1/db.csv ]; then \
	  echo "Falta ../e1/db.csv (generalo con el Ejercicio 1 o crea uno)"; exit 1; \
	fi
	@echo ">>> Server en 127.0.0.1:5000 usando ../e1/db.csv"
	@./bin/server -H 127.0.0.1 -p 5000 -n 4 -m 16 -f ../e1/db.csv

# Cliente interactivo
client: all
	@./bin/client -h 127.0.0.1 -p 5000

.PHONY: all clean demo_queue run_server client

