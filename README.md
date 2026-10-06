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

`GET /health` is a liveness probe. Point Prometheus at `host:9100` (`deploy/prometheus.yml`). The resulting Prometheus endpoint serves as a robust datasource for standard visualization and alerting systems, such as Grafana.

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

The workload generator (`lpt-workload`) produces synthetic CPU, memory, and I/O bottlenecks. The associated `perf` experiments require a Linux environment with appropriate hardware performance-counter support. They should be run on dedicated/stable hardware rather than a constrained laptop or a VM/WSL2 environment where PMU support may be unreliable.

The tables below describe the **theoretical/expected behavior** when profiling these synthetic workloads:

### 1. CPU-bound (Theoretical)

| Metric | Before (branchy FP loop) | After (`--optimized` integer mix) | What changed |
| --- | --- | --- | --- |
| Instructions / cycle (IPC) | Expected lower | Expected higher | Fewer mispredicted branches; tighter ALU loop |
| Branch-miss related stalls | Expected high (frontend) | Expected low | `if ((i & 1) == 0)` vs branch-light mix |
| `task-clock` ≈ wall | Time-bounded by workload | Time-bounded by workload | Both are designed to keep one workload thread busy |
| `lpt_cpu_usage_ratio{cpu="all"}` | High usage on single core | High usage on single core | Same occupancy; quality of those cycles differs |
| `lpt_context_switches_per_second` | Expected low | Expected low | CPU-bound, rarely blocks |

**Optimization:** replace a branchy `volatile double` reduction with an integer hash mix. `perf record` + `perf report` should show time moving out of library FP helpers into `burn_cpu_optimized`.

### 2. Memory-bound (Theoretical)

Working set memory allocation of anonymous `vector<uint64_t>`.

| Metric | Before (`--pattern rand`) | After (`--pattern seq --optimized`) | What changed |
| --- | --- | --- | --- |
| Cache pressure | High LLC thrashing | Better sequential locality and prefetch opportunity | Random vs sequential / prefetch |
| Page-faults | Initial spike, then ~0 | Initial spike, then ~0 | Both already mapped; faults are a **first-touch** story |
| Retired IPC | Expected lower | Expected higher | Stalled on DRAM vs streaming |
| `lpt_page_faults_minor_per_second` | Spike at start | Spike at start | Demand paging of anon pages (`do_anonymous_page`) |
| Memory allocation | Matches working set size | Matches working set size | Same RSS; different *access* pattern |

**Optimization:** sequential streaming lets the hardware prefetcher and adjacent cache lines work. Random index generation thrashes the TLB and LLC. First-touch minor faults are visible in `/proc/vmstat` `pgfault`; **cache** behavior is *not* in `/proc` — that is why the scripts use `perf stat`.

### 3. I/O-bound (Theoretical)

Writes into a target file on disk.

| Metric | Before (random writes) | After (`--optimized` sequential) | What changed |
| --- | --- | --- | --- |
| `lpt_disk_util_ratio` | High, bursty | High, smoother | Random I/O vs sequential |
| `lpt_disk_write_bytes_per_second` | Expected lower | Expected higher | Elevator / NVMe queue locality |
| `lpt_process_write_bytes_per_second`| Matches dirtied bytes | Matches dirtied bytes | `/proc/pid/io` is storage accounting |
| `lpt_process_voluntary_csw_per_second` | Expected higher | Expected lower | More blocking in `D` state on slow devices |
| `lpt_cpu_iowait_ratio` | Expected elevated | Expected lower | Scheduler iowait bucket |

Add `--sync` to force `fsync` after each write (durability-bound). That explodes `ms_io` and voluntary switches; the “after” sequential path without `fsync` is the page-cache streaming case.

### 4. Telemetry Parsing Optimization

**1. Problem**
The `/proc` collectors used `split_ws()`, `std::istringstream`, temporary strings/vectors, and tokenization in hot parsing paths.

**2. Optimization**
`apply_status_fields` and `apply_io_fields` were changed to use direct prefix matching and numeric parsing without unnecessary tokenization. `parse_proc_pid_stat` was changed to traverse fields directly after correctly locating the `comm` field.

**3. Correctness**
The existing GoogleTest suite passed: 9 tests passed.

**4. Benchmark Methodology**
This was an empirical test performed in a controlled WSL/Linux environment, not a production workload.
* 10 sequential baseline runs vs. 10 sequential optimized runs
* Configuration: 25 process limit, 50 ms interval, 5 second duration
* Measurement: POSIX `time` (no `perf` profiling overhead)
* Same hardware/environment for all runs

**5. Results (Median of 10 runs)**

| Metric | Baseline Median | Optimized Median | Change |
| --- | --- | --- | --- |
| User CPU time | 0.3035 s | 0.1775 s | 41.5% lower |
| System CPU time | 0.230 s | 0.232 s | Broadly unchanged |

**Observed Ranges:**
* Baseline user CPU: 0.274-0.360 s
* Optimized user CPU: 0.149-0.209 s

*(Note: Real elapsed time was excluded from the performance metrics because it is artificially bounded by the 5-second timeout.)*

**6. Interpretation**
The optimization primarily reduced userspace parsing overhead, while system CPU time remained broadly unchanged. This reflects total userspace CPU time for the controlled exporter benchmark.

**7. Reproduction**
```bash
{ time timeout 5 ./build/lpt-agent --listen 127.0.0.1 --port 9101 --process-limit 25 --interval-ms 50 ; }
```

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
