# Linux OS internals ↔ exporter metrics

This document maps each `lpt_*` Prometheus metric to the kernel accounting that produces it. The collector reads **text files** under `/proc` and `/sys` (via an injectable root, so tests can use fixtures). It does **not** use `perf_event_open`; `perf` is used separately in `scripts/` to attribute *why* a workload is slow.

## Sampling model

Most `/proc` counters are **monotonic**. The exporter stores the previous sample and publishes **rates**:

```
rate = (value_now - value_prev) / elapsed_seconds
```

CPU utilization is a special case: it is a ratio of *time-accounting deltas*, not wall-clock seconds (see below).

Clock ticks (`USER_HZ`, typically 100) come from `sysconf(_SC_CLK_TCK)`. Page size comes from `sysconf(_SC_PAGESIZE)`.

---

## CPU utilization — `/proc/stat`

Kernel file: `fs/proc/stat.c` (`show_stat`). Each `cpu` line is time spent in a scheduler/accounting bucket, in **USER_HZ** jiffies since boot.

| Field | Kernel meaning |
| --- | --- |
| `user` | Time in user mode (`CPUTIME_USER`), excluding guest |
| `nice` | User time with nice > 0 |
| `system` | Kernel mode (`CPUTIME_SYSTEM`): syscalls, kernel threads |
| `idle` | Idle loop (`CPUTIME_IDLE`) |
| `iowait` | Idle but at least one task on this CPU is waiting on block I/O |
| `irq` / `softirq` | Hard IRQ / softirq (NET_RX, TIMER, …) |
| `steal` | Time a hypervisor ran something else (guest only) |

**Busy** in this project: `user + nice + system + irq + softirq + steal`  
**Idle-all**: `idle + iowait`  
**`lpt_cpu_usage_ratio`**: `Δbusy / Δ(busy + idle-all)` for `cpu` (aggregate) and per `cpuN`.

This matches the usual “non-idle fraction” definition. It is **not** the same as `loadavg`, which counts runnable+uninterruptible tasks.

Related gauges:

- `lpt_cpu_user_ratio`, `lpt_cpu_system_ratio`, `lpt_cpu_iowait_ratio`, `lpt_cpu_irq_ratio`
- `lpt_procs_running` ← `procs_running` (runqueue length snapshot)
- `lpt_procs_blocked` ← `procs_blocked` (count of `TASK_UNINTERRUPTIBLE`, often I/O)

### Context switches — `ctxt`

`lpt_context_switches_per_second` is the delta of `/proc/stat` `ctxt`. The kernel increments this in the scheduler (`nr_context_switches`) on every actual context switch (voluntary `schedule()` and involuntary preemption). High rates with low CPU often mean **lock contention**, **blocking I/O**, or **over-subscription** of short-lived threads.

`lpt_forks_per_second` is `processes` (number of `fork`/`clone` completions).  
`lpt_interrupts_per_second` is the first field of `intr` (sum of all interrupts).

---

## Memory — `/proc/meminfo`

Kernel file: `fs/proc/meminfo.c`. Values are in **kB**.

| Metric | Source | Internals |
| --- | --- | --- |
| `lpt_memory_total_bytes` | `MemTotal` | Usable RAM (`totalram_pages`), not the full physical map |
| `lpt_memory_available_bytes` | `MemAvailable` | Estimate of pages reclaimable without swapping (free + cache − watermarks − unreclaimable slab) |
| `lpt_memory_used_ratio` | derived | `1 - MemAvailable/MemTotal` |
| `lpt_memory_anon_bytes` | `AnonPages` | Anonymous RSS: heaps, stacks, `MAP_ANONYMOUS` |
| `lpt_memory_cached_bytes` | `Cached` | Page cache for file-backed mappings (not including swap cache) |
| `lpt_memory_buffers_bytes` | `Buffers` | Block-device buffers (metadata/raw) |
| `lpt_memory_dirty_bytes` | `Dirty` | File pages modified but not yet written |
| `lpt_swap_used_bytes` | `SwapTotal - SwapFree` | Anonymous pages in the swapfile/partition |

`MemFree` alone is a poor “in use” signal: a healthy system often keeps free memory **low** because the page cache absorbs RAM. Prefer `MemAvailable`.

---

## Page faults and paging — `/proc/vmstat`

Kernel file: `mm/vmstat.c` / `fs/proc/meminfo.c` (`vmstat_text`). Counters are **event counts**, not bytes.

| Metric | Counter | Internals |
| --- | --- | --- |
| `lpt_page_faults_major_per_second` | `pgmajfault` | Fault that blocked to read from disk/swap (`VM_FAULT_MAJOR`) |
| `lpt_page_faults_minor_per_second` | `pgfault - pgmajfault` | Resolved without I/O: new anon zero page, COW, already in cache |
| `lpt_pages_in_per_second` | `pgpgin` | Pages read from block devices (historically kB; on modern kernels page units via vmstat) |
| `lpt_pages_out_per_second` | `pgpgout` | Pages written out (writeback / swap) |
| `lpt_swap_in_per_second` / `_out_` | `pswpin` / `pswpout` | Swap device traffic |

A **CPU-bound** tight loop that touches a small stack has almost no faults after startup. A **memory-bound** random walk over a multi-GB anonymous mapping generates **minor** faults on first touch (demand paging) then **cache misses** (visible in `perf stat` `cache-misses`, not in `pgfault`). **Major** faults spike when the working set is forced to disk or when you `mmap` a large file and read it.

---

## Disk I/O — `/proc/diskstats`

Kernel: `block/genhd.c` (`diskstats_show`), counters in `struct disk_stats`.

Format (v4.18+ still compatible with the 11-field core):

`reads_completed reads_merged sectors_read ms_reading writes_completed writes_merged sectors_written ms_writing ios_in_progress ms_io weighted_ms_io`

Sectors are **512 bytes** regardless of the device’s logical block size.

| Metric | Derivation |
| --- | --- |
| `lpt_disk_reads_per_second` | `Δreads_completed / s` |
| `lpt_disk_read_bytes_per_second` | `Δsectors_read * 512 / s` |
| `lpt_disk_util_ratio` | `Δms_io / (elapsed_ms)` clamped to 1 |
| `lpt_disk_avg_queue_depth` | `Δweighted_ms_io / elapsed_ms` (same idea as iostat `aqu-sz`) |

Partitions (`sda1`, `nvme0n1p1`) and `loop*`/`ram*` devices are skipped so Prometheus cardinality stays on whole disks.

`ios_in_progress` is a **gauge** of in-flight I/Os at sample time; utilization uses the **busy-time** accumulator `ms_io`.

---

## Process-level metrics — `/proc/<pid>/`

### `stat` (`fs/proc/array.c`)

The `comm` field is inside parentheses and may contain spaces; the parser uses the first `(` and last `)`.

| Field (after comm) | Use |
| --- | --- |
| `state` | `R/S/D/Z/T` … |
| `minflt` / `majflt` | Process minor/major faults (`lpt_process_*_faults_per_second`) |
| `utime` / `stime` | User/system jiffies → `lpt_process_cpu_ratio = Δ(utime+stime)/CLK_TCK/elapsed` (1.0 = one core) |
| `vsize` | Virtual address space bytes |
| `rss` | Resident pages × page size → `lpt_process_rss_bytes` |

### `status`

`Threads`, `voluntary_ctxt_switches`, `nonvoluntary_ctxt_switches` (same accounting as `nvcsw`/`nivcsw` in `stat` on recent kernels). Voluntary switches usually mean the task blocked (I/O, futex, sleep). Involuntary means the scheduler preempted it (CPU contention or timeslice).

### `io` (`fs/proc/base.c`, requires `CONFIG_TASK_IO_ACCOUNTING`)

`read_bytes` / `write_bytes` are **storage-level** bytes (after page cache), not `read()` syscall sizes. A write that hits the page cache and is later written back still shows up here when dirtied.

---

## What `/sys` is used for

The default collector is `/proc`-centric. Device names in `diskstats` correspond to `/sys/block/<dev>/`. You can extend collection with:

- `/sys/block/<dev>/queue/rotational` — HDD vs SSD
- `/sys/devices/system/cpu/cpuN/cpufreq/scaling_cur_freq` — p-state
- `/sys/fs/cgroup/` — container limits vs `MemAvailable`

Keep those as gauges with a `device` or `cpu` label if you add them; they are snapshots, not counters.

---

## Prometheus exposition

`GET /metrics` on the exporter (default `:9100`) returns [exposition format 0.0.4](https://prometheus.io/docs/instrumenting/exposition_formats/): `# HELP`, `# TYPE`, then `name{labels} value`. Gauges are used for rates so a restart does not require counter-reset handling. Counters are reserved for truly monotonic exported values if you add them later.

`GET /health` returns `ok` for liveness probes.
