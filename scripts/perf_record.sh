#!/usr/bin/env bash
# Record a perf.data file (call-graph) for a workload.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${LPT_WORKLOAD:-$ROOT/build/lpt-workload}"
OUT_DIR="${LPT_PERF_OUT:-$ROOT/results}"
mkdir -p "$OUT_DIR"

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 --type cpu|memory|io [workload args...]" >&2
  exit 2
fi

LABEL=$(echo "$*" | tr ' /' '__' | tr -cd 'A-Za-z0-9._=-')
DATA="$OUT_DIR/perf-${LABEL}.data"

# dwarf call graphs are more accurate than frame-pointer for unoptimized C++
# but dwarf is heavier; fp is fine if built with -fno-omit-frame-pointer.
FREQ="${LPT_PERF_FREQ:-99}"
echo "==> perf record -g -F $FREQ -o $DATA -- $BIN $*"
perf record -g -F "$FREQ" -o "$DATA" -- "$BIN" "$@"
echo "wrote $DATA"
echo "report with: $ROOT/scripts/perf_report.sh $DATA"
