#!/usr/bin/env bash
# Profile a workload with perf stat, perf record, and perf report.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${LPT_BIN:-$ROOT/build/lpt-workload}"
KIND="${1:-cpu}"          # cpu | memory | io
MODE="${2:-naive}"        # naive | optimized
SECONDS="${3:-8}"
OUTDIR="${LPT_OUT:-$ROOT/results/local}"
EVENTS="${PERF_EVENTS:-cycles,instructions,cache-references,cache-misses,context-switches,page-faults,task-clock,cpu-migrations,major-faults,minor-faults}"

if [[ ! -x "$BIN" ]]; then
  echo "Missing $BIN — build first (cmake -B build && cmake --build build)" >&2
  exit 1
fi

mkdir -p "$OUTDIR"
STAMP="$(date +%Y%m%d-%H%M%S)"
PREFIX="$OUTDIR/${KIND}_${MODE}_${STAMP}"

ARGS=( "$KIND" --mode "$MODE" --seconds "$SECONDS" )
if [[ "$KIND" == "memory" ]]; then
  ARGS+=( --size-mb "${SIZE_MB:-256}" )
fi
if [[ "$KIND" == "io" ]]; then
  ARGS+=( --io-path "${IO_PATH:-/tmp/lpt-workload.dat}" )
fi
if [[ -n "${THREADS:-}" ]]; then
  ARGS+=( --threads "$THREADS" )
fi

echo "==> perf stat $KIND/$MODE"
perf stat -e "$EVENTS" --output "${PREFIX}.stat.txt" -- "$BIN" "${ARGS[@]}"

echo "==> perf record (call-graph dwarf)"
perf record -g --call-graph dwarf -o "${PREFIX}.perf.data" -- "$BIN" "${ARGS[@]}"

echo "==> perf report"
perf report --stdio -i "${PREFIX}.perf.data" > "${PREFIX}.report.txt" || true

echo "Wrote:"
echo "  ${PREFIX}.stat.txt"
echo "  ${PREFIX}.perf.data"
echo "  ${PREFIX}.report.txt"
