#!/usr/bin/env bash
# Start the agent and print a few /metrics lines while a workload runs.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AGENT="${ROOT}/build/lpt-agent"
WL="${ROOT}/build/lpt-workload"
PORT="${PORT:-9100}"

"$AGENT" --listen 127.0.0.1 --port "$PORT" --interval-ms 500 &
AID=$!
trap 'kill $AID 2>/dev/null || true' EXIT
sleep 0.5

"$WL" cpu --mode naive --seconds 4 &
sleep 1
curl -s "http://127.0.0.1:${PORT}/metrics" | grep -E 'lpt_cpu_usage_ratio|lpt_context_switches_total|lpt_page_faults_total' | head
wait || true
