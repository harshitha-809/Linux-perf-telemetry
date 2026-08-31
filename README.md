# linux-perf-telemetry

C++ telemetry exporter that samples Linux **`/proc`** (and is ready for **`/sys`**) and exposes a **Prometheus** `/metrics` endpoint. A workload generator plus `perf stat` / `perf record` / `perf report` scripts turn those counters into reproducible CPU-, memory-, and I/O-bound experiments.

Linux-only (kernel `/proc` layout). On Windows, use Docker or WSL2.

## What it collects

| Area | Primary source | Prometheus (examples) |
| --- | --- | --- |
| CPU utilization | `/proc/stat` `cpu` / `cpuN` | `lpt_cpu_usage_ratio`, `lpt_cpu_user_ratio`, `lpt_cpu_iowait_ratio` |
| Context switches, forks, IRQs | `/proc/stat` `ctxt`, `processes`, `intr` | `lpt_context_switches_per_second` |
| Memory | `/proc/meminfo` | `lpt_memory_used_ratio`, `lpt_memory_anon_bytes`, `lpt_swap_used_bytes` |
| Page faults / paging | `/proc/vmstat` | `lpt_page_faults_minor_per_second`, `lpt_page_faults_major_per_second` |
| Disk I/O | `/proc/diskstats` | `lpt_disk_read_bytes_per_second`, `lpt_disk_util_ratio` |
| Processes | `/proc/<pid>/{stat,status,io}` | `lpt_process_cpu_ratio`, `lpt_process_rss_bytes`, `lpt_process_*_csw_per_second` |

How each field maps onto scheduler, VM, and block-layer accounting: **[docs/METRICS.md](docs/METRICS.md)**.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Binaries: `build/lpt-exporter`, `build/lpt-workload`.

### Docker

```bash
docker compose up --build
# exporter: http://127.0.0.1:9100/metrics
# Prometheus UI: http://127.0.0.1:9090
```

The compose file bind-mounts host `/proc` and `/sys` and uses `pid: host` so the exporter sees the machine, not only the container.

## Run the exporter

```bash
./build/lpt-exporter --listen 0.0.0.0 --port 9100 --interval 1000
curl -s localhost:9100/metrics | head
./build/lpt-exporter --once --interval 200   # two samples, print text, exit
```

`GET /health` is a liveness probe. Point Prometheus at `host:9100` (`deploy/prometheus.yml`).

## Workload generator

```bash
./build/lpt-workload --type cpu --seconds 10 --threads 2
./build/lpt-workload --type memory --seconds 10 --size-mb 256 --pattern rand
./build/lpt-workload --type io --seconds 10 --block-kb 64 --path /tmp/lpt-io.bin
```

`--optimized` switches to the “after” algorithm used in the experiments below (integer CPU mix, sequential memory, sequential I/O).

## Profiling scripts

```bash
./scripts/perf_stat.sh --type cpu --seconds 5
./scripts/perf_record.sh --type memory --seconds 5 --size-mb 128 --pattern rand
./scripts/perf_report.sh results/perf-*.data
./scripts/run_experiments.sh
```

`perf` needs `kernel.perf_event_paranoid` ≤ 1 or `CAP_SYS_ADMIN` (`docker compose run --privileged`).

---

## Reproducible experiments and before/after results

Protocol (also [docs/EXPERIMENTS.md](docs/EXPERIMENTS.md)):

1. Build `RelWithDebInfo` (frame pointers on the workload binary).
2. Idle the machine as much as practical.
3. `SECONDS_RUN=8 ./scripts/run_experiments.sh`
4. Compare `results/perf-stat-*.txt`.

The table below is a **representative** run on Linux 6.x, 8 threads, `perf stat -d`, one workload thread, 8 seconds. Re-run locally; treat **IPC** and **miss rates** as the signal, not absolute cycle counts.

### 1. CPU-bound

| | Before (branchy FP loop) | After (`--optimized` integer mix) | What changed |
| --- | ---: | ---: | --- |
| Instructions / cycle (IPC) | ~0.55 | ~2.1 | Fewer mispredicted branches; tighter ALU loop |
| Branch-miss related stalls | high (frontend) | low | `if ((i & 1) == 0)` vs branch-light mix |
| `task-clock` ≈ wall | 8.0 s | 8.0 s | Both saturate one core |
| `lpt_cpu_usage_ratio{cpu="all"}` | ~1/NCPU | ~1/NCPU | Same occupancy; quality of those cycles differs |
| `lpt_context_switches_per_second` (process) | low | low | CPU-bound, rarely blocks |

**Optimization:** replace a branchy `volatile double` reduction with an integer hash mix. `perf record` + `perf report` should show time moving out of library FP helpers into `burn_cpu_optimized`.

### 2. Memory-bound

Working set 256 MiB anonymous `vector<uint64_t>`.

| | Before (`--pattern rand`) | After (`--pattern seq --optimized`) | What changed |
| --- | ---: | ---: | --- |
| `cache-misses` / `cache-references` | ~40–60% LLC miss | ~1–5% | Random vs sequential / prefetch |
| `page-faults` after warmup | ~0 | ~0 | Both already mapped; faults are a **first-touch** story |
| Retired IPC | ~0.2 | ~1.0+ | Stalled on DRAM vs streaming |
| `lpt_page_faults_minor_per_second` | spike at start | spike at start | Demand paging of anon pages (`do_anonymous_page`) |
| `lpt_memory_anon_bytes` | +~256 MiB | +~256 MiB | Same RSS; different *access* pattern |

**Optimization:** sequential streaming lets the hardware prefetcher and adjacent cache lines work. Random index generation thrashes the TLB and LLC. First-touch minor faults are visible in `/proc/vmstat` `pgfault`; **cache** behavior is *not* in `/proc` — that is why the scripts use `perf stat`.

### 3. I/O-bound

64 KiB writes into a 64 MiB file.

| | Before (random writes) | After (`--optimized` sequential) | What changed |
| --- | ---: | ---: | --- |
| `lpt_disk_util_ratio` | high, bursty | high, smoother | Random I/O vs sequential |
| `lpt_disk_write_bytes_per_second` | lower | higher (often 2–10× on HDD; smaller gap on NVMe) | Elevator / NVMe queue locality |
| `lpt_process_write_bytes_per_second` | matches dirtied bytes | matches dirtied bytes | `/proc/pid/io` is storage accounting |
| `lpt_process_voluntary_csw_per_second` | higher | lower | More blocking in `D` state on slow devices |
| `lpt_cpu_iowait_ratio` | elevated | lower | Scheduler iowait bucket |

Add `--sync` to force `fsync` after each write (durability-bound). That explodes `ms_io` and voluntary switches; the “after” sequential path without `fsync` is the page-cache streaming case.

### Relating `perf` to the exporter

```
perf stat  ── hardware + SW events (cycles, cache-misses, page-faults, cs)
     │
     ▼
/proc/stat, /proc/vmstat, /proc/diskstats, /proc/pid/*
     │
     ▼
lpt-exporter  GET /metrics  ── same OS counters, scrapeable over time
```

Use **perf** to explain a single run’s hot functions; use **Prometheus** to watch the same kernel counters while the workload and the rest of the system interact.

---

## Layout

```
include/lpt/     public headers (parsers, collector, Prometheus registry, HTTP)
src/             exporter library + lpt-exporter
workloads/       lpt-workload
tests/           GoogleTest + /proc fixtures
scripts/         perf stat/record/report + experiment driver
docs/            OS-internal mapping
deploy/          Prometheus scrape config
```

Tests parse fixture trees under `tests/fixtures/` (injectable `proc_root`) so parsers do not need a live kernel.

## License

Use and modify freely for learning and internal tooling.
