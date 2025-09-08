# Makefile
CC      = gcc
CFLAGS  = -O2 -Wall -Wextra -Wpedantic -std=c11 -D_GNU_SOURCE
INCLUDES= -Iinclude
LDFLAGS = 
# En algunas distros antiguas, descomentá la siguiente línea:
# LDFLAGS += -lrt

# Toma todos los .c dentro de src/
#//SRC := $(wildcard src/*.c)
SRC = src/main.c src/common.c src/ipc.c src/ring.c src/generator.c src/coordinator.c
BIN = tp_e1

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(BIN)
	rm -f $(BIN) *.csv

.PHONY: demo
demo: $(BIN)
	@echo "== Demo pequeña =="
	./$(BIN) -n 2 -t 50 -o productos_small.csv -b 16
	@echo "== Demo stress concurrencia =="
	./$(BIN) -n 8 -t 2000 -o productos_stress.csv -b 128

