#!/usr/bin/env bash
# Run naive vs optimized workloads and capture perf stat for the README table.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export LPT_BIN="${LPT_BIN:-$ROOT/build/lpt-workload}"
export LPT_OUT="${LPT_OUT:-$ROOT/results/local}"
SECONDS="${SECONDS:-8}"

mkdir -p "$LPT_OUT"

echo "=== CPU bound ==="
"$ROOT/scripts/profile_workload.sh" cpu naive "$SECONDS"
"$ROOT/scripts/profile_workload.sh" cpu optimized "$SECONDS"

echo "=== Memory bound ==="
SIZE_MB="${SIZE_MB:-128}" "$ROOT/scripts/profile_workload.sh" memory naive "$SECONDS"
SIZE_MB="${SIZE_MB:-128}" "$ROOT/scripts/profile_workload.sh" memory optimized "$SECONDS"

echo "=== I/O bound ==="
"$ROOT/scripts/profile_workload.sh" io naive "$SECONDS"
"$ROOT/scripts/profile_workload.sh" io optimized "$SECONDS"

echo
echo "Compare IPC, cache-miss ratio, page-faults, and task-clock in $LPT_OUT/*.stat.txt"
echo "IPC ≈ instructions/cycles. Cache-miss ratio ≈ cache-misses/cache-references."
