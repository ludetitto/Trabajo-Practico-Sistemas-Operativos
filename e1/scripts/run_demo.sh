#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
make -j
rm -f db.csv
./bin/coordinador -n 2 -t 30 -f db.csv & COORD=$! # dejar correr el coordinador en background
sleep 0.2
./bin/generador -q 20 -g 0 & # dejar correr dos generadores en background
./bin/generador -q 20 -g 1 & # dejar correr dos generadores en background
# esperar a que terminen los generadores
wait $COORD || true
echo "---- db.csv ----"
cat db.csv
