#!/usr/bin/env bash
# Summarize a perf.data file (hot functions, overhead).
set -euo pipefail
DATA="${1:-}"
if [[ -z "$DATA" ]]; then
  echo "Usage: $0 <perf.data>" >&2
  exit 2
fi
OUT="${DATA%.data}-report.txt"

echo "==> perf report --stdio -n -i $DATA"
perf report --stdio -n --no-children -i "$DATA" | tee "$OUT"
echo
echo "==> top 20 symbols (children overhead)"
perf report --stdio -n -i "$DATA" | head -n 80 | tee -a "$OUT"
echo "wrote $OUT"
