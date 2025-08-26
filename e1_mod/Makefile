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

.PHONY: all clean
