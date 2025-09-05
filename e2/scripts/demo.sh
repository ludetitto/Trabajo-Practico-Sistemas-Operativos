#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
make -j
# lanza servidor en background
./bin/server -H 127.0.0.1 -p 5000 -n 2 -m 8 -f ../e1_mod/db.csv &
SRV=$!
sleep 0.3
# cliente 1 (transacción)
{
  printf "PING\nBEGIN\nADD nombre=Zeus generador=9 pid=1234\nUPDATE id=1 nombre=MOD1\nCOMMIT\nQUIT\n" | ./bin/client -h 127.0.0.1 -p 5000
} &
# cliente 2 (intentará consultar durante tx)
{
  sleep 0.5
  printf "GET 1\nFIND nombre=Zeus\nQUIT\n" | ./bin/client -h 127.0.0.1 -p 5000
} &
wait
kill $SRV || true
