CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Iinclude -pthread
SRCDIR = src
BINDIR = bin

all: $(BINDIR)/tp_gen

$(BINDIR)/tp_gen: $(SRCDIR)/tp_gen.c $(SRCDIR)/ipc_utils.c | $(BINDIR)
	$(CC) $(CFLAGS) $(SRCDIR)/tp_gen.c $(SRCDIR)/ipc_utils.c -o $@ -lrt -pthread

$(BINDIR):
	mkdir -p $(BINDIR)

clean:
	rm -rf $(BINDIR)
	rm -f salida.csv

.PHONY: all clean



