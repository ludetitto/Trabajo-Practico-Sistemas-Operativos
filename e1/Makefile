CC=gcc
CFLAGS=-O2 -Wall -Wextra -std=c11
LDFLAGS=-pthread
INCS=-Iinclude
SRC=src
OBJ=obj
BIN=bin

all: $(BIN)/coordinador $(BIN)/generador

$(BIN)/coordinador: $(OBJ)/coordinador.o $(OBJ)/ipc.o $(OBJ)/csv.o $(OBJ)/randrec.o
	@mkdir -p $(BIN)
	$(CC) $^ -o $@ $(LDFLAGS)

$(BIN)/generador: $(OBJ)/generador.o $(OBJ)/ipc.o $(OBJ)/csv.o $(OBJ)/randrec.o
	@mkdir -p $(BIN)
	$(CC) $^ -o $@ $(LDFLAGS)

$(OBJ)/%.o: $(SRC)/%.c include/*.h
	@mkdir -p $(OBJ)
	$(CC) $(CFLAGS) $(INCS) -c $< -o $@

clean:
	rm -rf $(OBJ) $(BIN)

.PHONY: all clean
