#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace lpt {

struct CpuTimes {
    std::string cpu_id;  // "cpu" or "cpu0"
    std::uint64_t user = 0;
    std::uint64_t nice = 0;
    std::uint64_t system = 0;
    std::uint64_t idle = 0;
    std::uint64_t iowait = 0;
    std::uint64_t irq = 0;
    std::uint64_t softirq = 0;
    std::uint64_t steal = 0;
    std::uint64_t guest = 0;
    std::uint64_t guest_nice = 0;

    std::uint64_t idle_all() const { return idle + iowait; }
    std::uint64_t non_idle() const {
        return user + nice + system + irq + softirq + steal;
    }
    std::uint64_t total() const { return idle_all() + non_idle(); }
};

struct CpuSnapshot {
    CpuTimes aggregate;
    std::vector<CpuTimes> per_cpu;
    std::uint64_t context_switches = 0;
    std::uint64_t processes_created = 0;
    std::uint64_t procs_running = 0;
    std::uint64_t procs_blocked = 0;
};

struct MemorySnapshot {
    std::uint64_t mem_total_kb = 0;
    std::uint64_t mem_free_kb = 0;
    std::uint64_t mem_available_kb = 0;
    std::uint64_t buffers_kb = 0;
    std::uint64_t cached_kb = 0;
    std::uint64_t swap_total_kb = 0;
    std::uint64_t swap_free_kb = 0;
    std::uint64_t anon_pages_kb = 0;
    std::uint64_t mapped_kb = 0;
    std::uint64_t shmem_kb = 0;
    std::uint64_t dirty_kb = 0;
    std::uint64_t writeback_kb = 0;
};

struct VmSnapshot {
    std::uint64_t pgfault = 0;
    std::uint64_t pgmajfault = 0;
    std::uint64_t pswpin = 0;
    std::uint64_t pswpout = 0;
    std::uint64_t pgpgin = 0;
    std::uint64_t pgpgout = 0;
    std::uint64_t nr_dirty = 0;
    std::uint64_t nr_writeback = 0;
    std::uint64_t numa_hit = 0;
    std::uint64_t numa_miss = 0;
};

struct DiskDevice {
    std::string name;
    std::uint64_t reads_completed = 0;
    std::uint64_t reads_merged = 0;
    std::uint64_t sectors_read = 0;
    std::uint64_t read_time_ms = 0;
    std::uint64_t writes_completed = 0;
    std::uint64_t writes_merged = 0;
    std::uint64_t sectors_written = 0;
    std::uint64_t write_time_ms = 0;
    std::uint64_t io_in_progress = 0;
    std::uint64_t io_time_ms = 0;
    std::uint64_t weighted_io_time_ms = 0;
};

struct DiskSnapshot {
    std::vector<DiskDevice> devices;
};

struct ProcessSample {
    int pid = 0;
    std::string comm;
    char state = '?';
    std::uint64_t utime = 0;
    std::uint64_t stime = 0;
    std::uint64_t cutime = 0;
    std::uint64_t cstime = 0;
    std::int64_t num_threads = 0;
    std::uint64_t vsize = 0;
    std::int64_t rss_pages = 0;
    std::uint64_t minflt = 0;
    std::uint64_t majflt = 0;
    std::uint64_t voluntary_ctxt_switches = 0;
    std::uint64_t nonvoluntary_ctxt_switches = 0;
    std::uint64_t read_bytes = 0;
    std::uint64_t write_bytes = 0;
};

struct ProcessSnapshot {
    std::vector<ProcessSample> processes;
};

struct SystemSnapshot {
    CpuSnapshot cpu;
    MemorySnapshot memory;
    VmSnapshot vm;
    DiskSnapshot disk;
    ProcessSnapshot processes;
};

struct CpuUtilization {
    std::string cpu_id;
    double usage_ratio = 0.0;  // 0..1
    double user_ratio = 0.0;
    double system_ratio = 0.0;
    double iowait_ratio = 0.0;
    double irq_ratio = 0.0;
};

inline CpuUtilization utilization_from_delta(const CpuTimes& prev, const CpuTimes& cur) {
    CpuUtilization u;
    u.cpu_id = cur.cpu_id;
    const double dt = static_cast<double>(cur.total() - prev.total());
    if (dt <= 0.0) {
        return u;
    }
    auto ratio = [&](std::uint64_t a, std::uint64_t b) {
        return static_cast<double>(b - a) / dt;
    };
    u.usage_ratio = 1.0 - ratio(prev.idle_all(), cur.idle_all());
    u.user_ratio = ratio(prev.user + prev.nice, cur.user + cur.nice);
    u.system_ratio = ratio(prev.system, cur.system);
    u.iowait_ratio = ratio(prev.iowait, cur.iowait);
    u.irq_ratio = ratio(prev.irq + prev.softirq, cur.irq + cur.softirq);
    if (u.usage_ratio < 0.0) {
        u.usage_ratio = 0.0;
    }
    if (u.usage_ratio > 1.0) {
        u.usage_ratio = 1.0;
    }
    return u;
}

}  // namespace lpt
