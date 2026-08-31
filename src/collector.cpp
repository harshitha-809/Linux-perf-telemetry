#include "lpt/collector.hpp"

#include "lpt/cpu.hpp"
#include "lpt/disk.hpp"
#include "lpt/memory.hpp"
#include "lpt/process.hpp"
#include "lpt/vmstat.hpp"

#ifndef _WIN32
#include <unistd.h>
#endif

namespace lpt {
namespace {

long clk_tck() {
#ifdef _WIN32
    return 100;
#else
    long v = ::sysconf(_SC_CLK_TCK);
    return v > 0 ? v : 100;
#endif
}

long page_size() {
#ifdef _WIN32
    return 4096;
#else
    long v = ::sysconf(_SC_PAGESIZE);
    return v > 0 ? v : 4096;
#endif
}

}  // namespace

Collector::Collector(CollectorConfig cfg)
    : cfg_(std::move(cfg)), reader_(cfg_.proc_root) {}

void Collector::scrape() {
    std::lock_guard<std::mutex> lock(mu_);
    const auto now = std::chrono::steady_clock::now();
    CpuSnapshot cpu = collect_cpu(reader_);
    MemoryInfo mem = collect_memory(reader_);
    VmstatCounters vm = collect_vmstat(reader_);
    auto disk = collect_disk(reader_);
    std::vector<ProcessSample> procs;
    if (cfg_.collect_processes) {
        procs = collect_processes(reader_, cfg_.max_processes);
    }

    last_memory_ = mem;
    if (have_prev_) {
        const double sec = std::chrono::duration<double>(now - prev_tp_).count();
        last_cpu_ = compute_cpu_rates(prev_cpu_, cpu, sec);
        last_vmstat_ = compute_vmstat_rates(prev_vm_, vm, sec);
        last_disk_ = compute_disk_rates(prev_disk_, disk, sec);
        last_procs_ = compute_process_rates(prev_proc_, procs, sec, clk_tck(), page_size());
    }
    publish_locked();

    prev_cpu_ = std::move(cpu);
    prev_vm_ = std::move(vm);
    prev_disk_ = std::move(disk);
    prev_proc_ = std::move(procs);
    prev_tp_ = now;
    have_prev_ = true;
}

void Collector::publish_locked() {
    registry_.set_gauge("lpt_cpu_usage_ratio", "CPU busy time fraction (user+system+irq+steal)",
                        last_cpu_.usage_ratio, {{"cpu", "all"}});
    registry_.set_gauge("lpt_cpu_user_ratio", "CPU user-mode time fraction", last_cpu_.user_ratio,
                        {{"cpu", "all"}});
    registry_.set_gauge("lpt_cpu_system_ratio", "CPU kernel-mode time fraction", last_cpu_.system_ratio,
                        {{"cpu", "all"}});
    registry_.set_gauge("lpt_cpu_iowait_ratio", "CPU iowait time fraction", last_cpu_.iowait_ratio,
                        {{"cpu", "all"}});
    registry_.set_gauge("lpt_cpu_irq_ratio", "CPU hard+soft IRQ time fraction", last_cpu_.irq_ratio,
                        {{"cpu", "all"}});
    for (std::size_t i = 0; i < last_cpu_.per_cpu_usage.size(); ++i) {
        registry_.set_gauge("lpt_cpu_usage_ratio", "CPU busy time fraction (user+system+irq+steal)",
                            last_cpu_.per_cpu_usage[i], {{"cpu", std::to_string(i)}});
    }
    registry_.set_gauge("lpt_context_switches_per_second",
                        "System-wide context switches per second from /proc/stat ctxt",
                        last_cpu_.context_switches_per_sec);
    registry_.set_gauge("lpt_forks_per_second", "Process creations per second from /proc/stat processes",
                        last_cpu_.forks_per_sec);
    registry_.set_gauge("lpt_interrupts_per_second", "Interrupts per second from /proc/stat intr",
                        last_cpu_.interrupts_per_sec);
    registry_.set_gauge("lpt_procs_running", "Tasks in the run queue",
                        static_cast<double>(prev_cpu_.procs_running));
    registry_.set_gauge("lpt_procs_blocked", "Tasks blocked on I/O",
                        static_cast<double>(prev_cpu_.procs_blocked));

    registry_.set_gauge("lpt_memory_total_bytes", "MemTotal from /proc/meminfo",
                        static_cast<double>(last_memory_.mem_total_kb) * 1024.0);
    registry_.set_gauge("lpt_memory_available_bytes", "MemAvailable from /proc/meminfo",
                        static_cast<double>(last_memory_.mem_available_kb) * 1024.0);
    registry_.set_gauge("lpt_memory_used_ratio", "1 - MemAvailable/MemTotal",
                        memory_used_ratio(last_memory_));
    registry_.set_gauge("lpt_memory_anon_bytes", "Anonymous RSS (user heaps/stacks)",
                        static_cast<double>(last_memory_.anon_pages_kb) * 1024.0);
    registry_.set_gauge("lpt_memory_cached_bytes", "Page cache (Cached)",
                        static_cast<double>(last_memory_.cached_kb) * 1024.0);
    registry_.set_gauge("lpt_memory_buffers_bytes", "Block device buffers",
                        static_cast<double>(last_memory_.buffers_kb) * 1024.0);
    registry_.set_gauge("lpt_memory_dirty_bytes", "Dirty pages waiting for writeback",
                        static_cast<double>(last_memory_.dirty_kb) * 1024.0);
    registry_.set_gauge("lpt_swap_used_bytes", "SwapTotal - SwapFree",
                        static_cast<double>(last_memory_.swap_total_kb - last_memory_.swap_free_kb) *
                            1024.0);

    registry_.set_gauge("lpt_page_faults_minor_per_second",
                        "Minor page faults/s (pgfault - pgmajfault)",
                        last_vmstat_.minor_faults_per_sec);
    registry_.set_gauge("lpt_page_faults_major_per_second", "Major page faults/s (pgmajfault)",
                        last_vmstat_.major_faults_per_sec);
    registry_.set_gauge("lpt_pages_in_per_second", "pgpgin (KiB units historically, pages in 2.4+)",
                        last_vmstat_.pages_in_per_sec);
    registry_.set_gauge("lpt_pages_out_per_second", "pgpgout", last_vmstat_.pages_out_per_sec);
    registry_.set_gauge("lpt_swap_in_per_second", "pswpin", last_vmstat_.swap_in_per_sec);
    registry_.set_gauge("lpt_swap_out_per_second", "pswpout", last_vmstat_.swap_out_per_sec);

    for (const auto& d : last_disk_) {
        const auto lbl = std::map<std::string, std::string>{{"device", d.name}};
        registry_.set_gauge("lpt_disk_reads_per_second", "Completed reads/s from /proc/diskstats",
                            d.reads_per_sec, lbl);
        registry_.set_gauge("lpt_disk_writes_per_second", "Completed writes/s from /proc/diskstats",
                            d.writes_per_sec, lbl);
        registry_.set_gauge("lpt_disk_read_bytes_per_second", "Read throughput (512-byte sectors)",
                            d.read_bytes_per_sec, lbl);
        registry_.set_gauge("lpt_disk_write_bytes_per_second", "Write throughput (512-byte sectors)",
                            d.write_bytes_per_sec, lbl);
        registry_.set_gauge("lpt_disk_util_ratio", "Fraction of time the device had I/O in flight",
                            d.util_ratio, lbl);
        registry_.set_gauge("lpt_disk_avg_queue_depth", "Average queue depth from weighted I/O time",
                            d.avg_queue_depth, lbl);
    }

    std::size_t n = 0;
    for (const auto& p : last_procs_) {
        if (n++ >= cfg_.max_processes) {
            break;
        }
        const auto lbl = std::map<std::string, std::string>{
            {"pid", std::to_string(p.pid)}, {"comm", p.comm}};
        registry_.set_gauge("lpt_process_cpu_ratio", "Process CPU time / elapsed (1.0 = one core)",
                            p.cpu_ratio, lbl);
        registry_.set_gauge("lpt_process_rss_bytes", "Resident set from /proc/pid/stat rss * page size",
                            static_cast<double>(p.rss_bytes), lbl);
        registry_.set_gauge("lpt_process_vsize_bytes", "Virtual size from /proc/pid/stat",
                            static_cast<double>(p.vsize_bytes), lbl);
        registry_.set_gauge("lpt_process_minor_faults_per_second", "minflt delta",
                            p.minor_faults_per_sec, lbl);
        registry_.set_gauge("lpt_process_major_faults_per_second", "majflt delta",
                            p.major_faults_per_sec, lbl);
        registry_.set_gauge("lpt_process_read_bytes_per_second", "read_bytes from /proc/pid/io",
                            p.read_bytes_per_sec, lbl);
        registry_.set_gauge("lpt_process_write_bytes_per_second", "write_bytes from /proc/pid/io",
                            p.write_bytes_per_sec, lbl);
        registry_.set_gauge("lpt_process_voluntary_csw_per_second", "voluntary_ctxt_switches",
                            p.voluntary_csw_per_sec, lbl);
        registry_.set_gauge("lpt_process_involuntary_csw_per_second", "nonvoluntary_ctxt_switches",
                            p.involuntary_csw_per_sec, lbl);
        registry_.set_gauge("lpt_process_threads", "Threads from /proc/pid/status",
                            static_cast<double>(p.threads), lbl);
    }
}

std::string Collector::prometheus_text() const {
    std::lock_guard<std::mutex> lock(mu_);
    return registry_.render_prometheus();
}

}  // namespace lpt
