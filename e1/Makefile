CC=gcc
CFLAGS=-O2 -Wall -Wextra -std=c11 -Iinclude
LDFLAGS=-pthread
OBJDIR=obj
BINDIR=bin

# Build por defecto: generador y coordinador
all: $(BINDIR)/coordinador $(BINDIR)/generador

# Regla patrón para compilar cualquier .c de src/ a .o en obj/
$(OBJDIR)/%.o: src/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Link del coordinador: ahora incluye cola.o y eventos.o
$(BINDIR)/coordinador: \
	$(OBJDIR)/coordinador.o \
	$(OBJDIR)/ipc.o \
	$(OBJDIR)/csv.o \
	$(OBJDIR)/cola.o \
	$(OBJDIR)/eventos.o \
	| $(BINDIR)
	$(CC) $^ -o $@ $(LDFLAGS)

# Link del generador
$(BINDIR)/generador: \
	$(OBJDIR)/generador.o \
	$(OBJDIR)/ipc.o \
	$(OBJDIR)/randrec.o \
	| $(BINDIR)
	$(CC) $^ -o $@ $(LDFLAGS)

# Crear carpetas de build si no existen
$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

# Limpieza
clean:
	rm -rf $(OBJDIR) $(BINDIR)

# Demo rápida con 6 generadores y 50 registros (muestra db.csv y cola.csv si existe)
run: all
	@echo ">>> Ejecutando demo con 6 generadores (50 registros)"
	@rm -f db.csv cola.csv
	@./bin/coordinador -n 6 -t 60 -f db.csv & \
	sleep 2; \
	for i in $(shell seq 1 6); do \
		./bin/generador -q 10 -g $$i & \
	done; \
	wait; \
	echo ">>> Registros generados:"; wc -l db.csv; \
	echo ">>> Primeras filas:"; head db.csv; \
	echo ">>> Últimas  filas:"; tail db.csv; \
	if [ -f cola.csv ]; then \
	  echo ">>> Cola (primeras líneas):"; head -n 10 cola.csv; \
	  echo ">>> Cola (últimas  líneas):"; tail -n 10 cola.csv; \
	fi

.PHONY: all clean run
