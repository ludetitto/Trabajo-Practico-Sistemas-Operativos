#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
make -j
rm -f db.csv
./bin/coordinador -n 2 -t 30 -f db.csv & COORD=$!
sleep 0.2
./bin/generador -q 20 -g 0 &
./bin/generador -q 20 -g 1 &
wait $COORD || true
echo "---- db.csv ----"
cat db.csv
