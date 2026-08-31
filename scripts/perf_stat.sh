#!/usr/bin/env bash
# Profile a workload with perf stat (high-level HW/SW counters).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${LPT_WORKLOAD:-$ROOT/build/lpt-workload}"
OUT_DIR="${LPT_PERF_OUT:-$ROOT/results}"
mkdir -p "$OUT_DIR"

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 --type cpu|memory|io [workload args...] [--optimized]" >&2
  exit 2
fi

LABEL=$(echo "$*" | tr ' /' '__' | tr -cd 'A-Za-z0-9._=-')
OUT="$OUT_DIR/perf-stat-${LABEL}.txt"

echo "==> perf stat $*" | tee "$OUT"
# -d: extra cache stats; -e: software events that map to VM/scheduler internals
perf stat -d \
  -e cycles,instructions,cache-misses,cache-references \
  -e context-switches,cpu-migrations,page-faults,task-clock \
  -- "$BIN" "$@" 2>&1 | tee -a "$OUT"
echo "wrote $OUT"
