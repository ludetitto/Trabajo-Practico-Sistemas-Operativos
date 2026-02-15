CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -pthread
BIN_DIR = bin

SRCS_SERVER = src/main_server.c src/server.c src/csv_db.c src/protocol.c src/utils.c
SRCS_CLIENT = src/main_client.c src/client.c src/protocol.c src/utils.c

.PHONY: all clean

all: $(BIN_DIR) $(BIN_DIR)/server $(BIN_DIR)/client

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BIN_DIR)/server: $(SRCS_SERVER)
	$(CC) $(CFLAGS) -o $@ $^

$(BIN_DIR)/client: $(SRCS_CLIENT)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -rf $(BIN_DIR)



