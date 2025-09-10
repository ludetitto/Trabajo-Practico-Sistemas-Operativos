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

