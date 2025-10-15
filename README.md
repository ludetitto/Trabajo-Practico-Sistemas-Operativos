# Trabajo Práctico – Sistemas Operativos
Trabajo práctico integrador (UNLaM).  

Maria Celeste Torres Moran - 44005719
Lucia Macarena De Titto - 46501934
Rocio De Jesus - 44726983
Francisco Nahuel Vignardel Villagra - 45778667
Gustavo Gabriel  Ayala Bustos - 44109078
Martín Palacios - 42932788
---

## Ejercicio 1 — Generador (procesos + SHM)

### Objetivo
Un coordinador asigna IDs y escribe un CSV; N generadores producen registros vía memoria compartida con semáforos (productor–consumidor).

### Cómo compilar
```bash
cd e1
make clean && make
```

### Comandos Make (e1)
| Comando | Descripción |
| --- | --- |
| `make` | Compila (bin/main) |
| `make test` | Ejecuta todos los casos de prueba |
| `make test1..test7` | Ejecuta un caso puntual |
| `make clean` | Borra objetos/binarios |
| `make clean-tests` | Limpia logs/CSVs de pruebas |

Salida: logs en `e1/logs/` y CSVs de prueba donde corresponde.

---

## Ejercicio 2 — Servidor CSV (sockets + TX)

### Objetivo
Servidor multi‑thread con transacciones y locks sobre un archivo CSV; cliente interactivo via TCP.

### Cómo compilar
```bash
cd e2
make clean && make
```

### Probar (modo guiado)
- En una terminal: `make -C e2 testN` (N=1..7). Inicia el server en primer plano (puertos 5001..5007) y muestra el paso a paso.
- En otra terminal (ubicado en `e2/`): `./bin/client -h 127.0.0.1 -p <puerto>`.
- Escribí las líneas que sugiere el test; a la derecha tenés el esperado (# => ...).
- Para detener el server: Ctrl+C en la terminal del test.

### Respuestas clave (servidor)
- `PING` → `OK` | `QUIT` → `BYE`
- `BEGIN` → `OK` (o `ERR TX_ACTIVE` si hay otra TX)
- `COMMIT` → `OK` (o `ERR NOT_OWNER_OR_NO_TX`)
- `ROLLBACK` → `OK` (o `ERR NOT_OWNER_OR_NO_TX`)
- `ADD nombre=.. precio=.. stock=..` → `OK` | `ERR ARG` si faltan campos | `ERR NOT_TX_ACTIVE` si no hay TX | `ERR TX_ACTIVE` si la TX es de otro
- `GET/FIND` → `RESULT ...` o `ERR NOT_FOUND`; `FIND ALL` → `ROW ...`* y `END`

### Validación (pasa/falla) breve
- Test 1–2: `PING` → `OK`; `QUIT` → `BYE` en cada cliente.
- Test 3: Por cliente: `BEGIN`/`ADD`/`COMMIT` → `OK`; si otro intenta `BEGIN` en paralelo → `ERR TX_ACTIVE` hasta liberar.
- Test 4: `BEGIN`/`ADD`/`ROLLBACK` → `OK`; `COMMIT` luego del rollback → `ERR NOT_OWNER_OR_NO_TX`.
- Test 5: Comando inválido → `ERR CMD`; `ADD` incompleto → `ERR ARG`; flujo completo termina en `BYE`.
- Test 6: Tras matar el server con TX abierta, al reiniciar `FIND` del item no muestra `RESULT` (rollback).
- Test 7: Varios clientes logran `BEGIN/ADD/COMMIT/QUIT` con `OK/BYE`; colisiones ven `ERR TX_ACTIVE` y luego pasan.