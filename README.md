# Trabajo Práctico – Sistemas Operativos
Trabajo práctico integrador para la materia **Sistemas Operativos** (UNLaM).

> **Documentación ampliada:**  
> [Google Doc](https://docs.google.com/document/d/1WsCG5847YD7xCmM5lIUzmT49tYR9RAcClLN4aHCAf1w/edit?usp=sharing)

---

## Ejercicio 1 — Generador de datos con procesos y SHM

### Objetivo (una línea)
Un **coordinador** asigna IDs y escribe un **CSV**; **N generadores** crean registros y se los envían **uno por vez** por **memoria compartida** usando **semáforos** (patrón productor–consumidor).

### Requisitos previos
Ubuntu (WSL o nativo) con:
```bash
sudo apt-get update -y
sudo apt-get install -y build-essential gawk


cd Ejercicio1
make clean && make