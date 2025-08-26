#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

N="${N:-3}"
TOTAL="${TOTAL:-100}"

make -s clean && make -s
echo "▶ ./ej1 $N $TOTAL"
./ej1 "$N" "$TOTAL"

echo "▶ Validando IDs en registros.csv"
awk -f scripts/awk_idcheck.awk registros.csv
echo "✔ Demo OK → registros.csv"
