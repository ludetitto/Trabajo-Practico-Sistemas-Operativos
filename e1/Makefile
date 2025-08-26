CC=gcc
CFLAGS=-O2 -Wall -Wextra -std=c11 -Iinclude
LDFLAGS=-pthread
OBJDIR=obj
BINDIR=bin

SRC=$(wildcard src/*.c)
OBJ=$(patsubst src/%.c,$(OBJDIR)/%.o,$(SRC))

all: $(BINDIR)/coordinador $(BINDIR)/generador

$(OBJDIR)/%.o: src/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BINDIR)/coordinador: $(OBJDIR)/coordinador.o $(OBJDIR)/ipc.o $(OBJDIR)/csv.o | $(BINDIR)
	$(CC) $^ -o $@ $(LDFLAGS)

$(BINDIR)/generador: $(OBJDIR)/generador.o $(OBJDIR)/ipc.o $(OBJDIR)/randrec.o | $(BINDIR)
	$(CC) $^ -o $@ $(LDFLAGS)

$(OBJDIR):
	mkdir -p $(OBJDIR)
$(BINDIR):
	mkdir -p $(BINDIR)

clean:
	rm -rf $(OBJDIR) $(BINDIR)


# Demo rápida con 3 generadores y 30 registros
run: all
	@echo ">>> Ejecutando prueba demo con 3 generadores (30 registros)"
	@rm -f db.csv
	@./bin/coordinador -n 3 -t 30 -f db.csv & \
	sleep 0.5; \
	./bin/generador -q 10 -g 0 & \
	./bin/generador -q 10 -g 1 & \
	./bin/generador -q 10 -g 2 & \
	wait; \
	echo ">>> Registros generados:"; wc -l db.csv; \
	echo ">>> Primeras filas:"; head db.csv; \
	echo ">>> Últimas filas:"; tail db.csv



.PHONY: all clean
