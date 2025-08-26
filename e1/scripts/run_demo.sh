#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
rm -f db.csv
./bin/coordinador -n 3 -t 100 -f db.csv & PID=$!
sleep 0.5
./bin/generador -q 40 &
./bin/generador -q 30 &
./bin/generador -q 30 &
wait $PID
echo "Hecho. Salida: db.csv"
