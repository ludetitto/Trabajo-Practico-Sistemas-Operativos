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

# Demo rápida con 3 generadores y 30 registros (muestra db.csv y cola.csv si existe)
run: all
	@echo ">>> Ejecutando demo con 3 generadores (30 registros)"
	@rm -f db.csv cola.csv
	@./bin/coordinador -n 3 -t 30 -f db.csv & \
	sleep 0.5; \
	./bin/generador -q 10 -g 0 & \
	./bin/generador -q 10 -g 1 & \
	./bin/generador -q 10 -g 2 & \
	wait; \
	echo ">>> Registros generados:"; wc -l db.csv; \
	echo ">>> Primeras filas:"; head db.csv; \
	echo ">>> Últimas  filas:"; tail db.csv; \
	if [ -f cola.csv ]; then \
	  echo ">>> Cola (primeras líneas):"; head -n 10 cola.csv; \
	  echo ">>> Cola (últimas  líneas):"; tail -n 10 cola.csv; \
	fi

.PHONY: all clean run
