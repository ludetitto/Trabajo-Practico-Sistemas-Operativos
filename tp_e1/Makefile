# Makefile
CC      = gcc
CFLAGS  = -O2 -Wall -Wextra -Wpedantic -std=c11 -D_GNU_SOURCE
INCLUDES= -Iinclude
LDFLAGS = 
# En algunas distros antiguas, descomentá la siguiente línea:
# LDFLAGS += -lrt

SRC = src/main.c src/common.c src/ipc.c src/ring.c src/producer.c src/coordinator.c
BIN = tp_e1

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(BIN)
