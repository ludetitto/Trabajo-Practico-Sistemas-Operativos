# Nombre del ejecutable
TARGET = ej1 

# Archivo fuente
SRC = ej1.c

# Flags de compilación
CFLAGS = -Wall -pthread -lrt

# Default: compila todo
all: $(TARGET)

# Regla para compilar
$(TARGET): $(SRC)
	gcc $(SRC) -o $(TARGET) $(CFLAGS)

# Ejecutar el programa (ejemplo 3 generadores, 50 registros)
run: $(TARGET)
	./$(TARGET) 3 50

# Limpiar ejecutables y archivos temporales
clean:
	rm -f $(TARGET) registros.csv

