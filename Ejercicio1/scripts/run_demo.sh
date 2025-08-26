#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

N="${N:-3}"
TOTAL="${TOTAL:-100}"
CSV="${CSV:-registros.csv}"
DELAY_MS="${DELAY_MS:-0}"

make -s clean && make -s
./ej1 -n "$N" -t "$TOTAL" -f "$CSV" -d "$DELAY_MS"
awk -f scripts/awk_idcheck.awk "$CSV"
echo "✔ Demo OK → $CSV"
