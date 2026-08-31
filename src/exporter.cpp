#include "lpt/exporter.hpp"

namespace lpt {

SnapshotExporter::SnapshotExporter(std::size_t process_limit) : process_limit_(process_limit) {}

void SnapshotExporter::update(const SystemSnapshot& snap) {
    registry_.clear();

    registry_.set_counter("lpt_context_switches_total",
                     "Cumulative context switches from /proc/stat ctxt (scheduler runqueue hops)",
                     static_cast<double>(snap.cpu.context_switches));
    registry_.set_counter("lpt_forks_total", "Processes created (fork/clone) from /proc/stat processes",
                     static_cast<double>(snap.cpu.processes_created));
    registry_.set_gauge("lpt_procs_running", "Runnable tasks from /proc/stat procs_running",
                   static_cast<double>(snap.cpu.procs_running));
    registry_.set_gauge("lpt_procs_blocked", "Uninterruptible sleep count from /proc/stat procs_blocked",
                   static_cast<double>(snap.cpu.procs_blocked));

    auto emit_cpu_times = [&](const CpuTimes& t) {
        const Labels cpu{{"cpu", t.cpu_id}};
        auto add = [&](const char* mode, std::uint64_t ticks) {
            Labels l = cpu;
            l.emplace("mode", mode);
            registry_.set_counter("lpt_cpu_ticks_total",
                             "CPU time in USER_HZ ticks from /proc/stat (CFS accounting)",
                             static_cast<double>(ticks), l);
        };
        add("user", t.user);
        add("nice", t.nice);
        add("system", t.system);
        add("idle", t.idle);
        add("iowait", t.iowait);
        add("irq", t.irq);
        add("softirq", t.softirq);
        add("steal", t.steal);
    };
    emit_cpu_times(snap.cpu.aggregate);
    for (const auto& t : snap.cpu.per_cpu) {
        emit_cpu_times(t);
    }

    if (have_prev_cpu_) {
        const auto u = utilization_from_delta(prev_cpu_.aggregate, snap.cpu.aggregate);
        registry_.set_gauge("lpt_cpu_usage_ratio",
                       "Fraction of non-idle CPU time since the previous scrape", u.usage_ratio,
                       {{"cpu", "cpu"}});
        registry_.set_gauge("lpt_cpu_user_ratio", "User+nice time fraction since previous scrape",
                       u.user_ratio, {{"cpu", "cpu"}});
        registry_.set_gauge("lpt_cpu_system_ratio", "Kernel time fraction since previous scrape",
                       u.system_ratio, {{"cpu", "cpu"}});
        registry_.set_gauge("lpt_cpu_iowait_ratio", "I/O wait fraction since previous scrape",
                       u.iowait_ratio, {{"cpu", "cpu"}});
        for (std::size_t i = 0; i < snap.cpu.per_cpu.size() && i < prev_cpu_.per_cpu.size(); ++i) {
            const auto pu = utilization_from_delta(prev_cpu_.per_cpu[i], snap.cpu.per_cpu[i]);
            registry_.set_gauge("lpt_cpu_usage_ratio",
                           "Fraction of non-idle CPU time since the previous scrape", pu.usage_ratio,
                           {{"cpu", pu.cpu_id}});
        }
    }

    const double kb = 1024.0;
    registry_.set_gauge("lpt_memory_bytes", "Memory from /proc/meminfo",
                   static_cast<double>(snap.memory.mem_total_kb) * kb, {{"state", "total"}});
    registry_.set_gauge("lpt_memory_bytes", "Memory from /proc/meminfo",
                   static_cast<double>(snap.memory.mem_free_kb) * kb, {{"state", "free"}});
    registry_.set_gauge("lpt_memory_bytes", "Memory from /proc/meminfo",
                   static_cast<double>(snap.memory.mem_available_kb) * kb, {{"state", "available"}});
    registry_.set_gauge("lpt_memory_bytes", "Memory from /proc/meminfo",
                   static_cast<double>(snap.memory.buffers_kb) * kb, {{"state", "buffers"}});
    registry_.set_gauge("lpt_memory_bytes", "Memory from /proc/meminfo",
                   static_cast<double>(snap.memory.cached_kb) * kb, {{"state", "cached"}});
    registry_.set_gauge("lpt_memory_bytes", "Memory from /proc/meminfo",
                   static_cast<double>(snap.memory.anon_pages_kb) * kb, {{"state", "anon"}});
    registry_.set_gauge("lpt_memory_bytes", "Memory from /proc/meminfo",
                   static_cast<double>(snap.memory.dirty_kb) * kb, {{"state", "dirty"}});
    registry_.set_gauge("lpt_swap_bytes", "Swap from /proc/meminfo",
                   static_cast<double>(snap.memory.swap_total_kb) * kb, {{"state", "total"}});
    registry_.set_gauge("lpt_swap_bytes", "Swap from /proc/meminfo",
                   static_cast<double>(snap.memory.swap_free_kb) * kb, {{"state", "free"}});

    registry_.set_counter("lpt_page_faults_total",
                     "Page faults from /proc/vmstat (minor includes major)",
                     static_cast<double>(snap.vm.pgfault), {{"type", "minor_plus_major"}});
    registry_.set_counter("lpt_page_faults_total", "Major faults requiring disk I/O (pgmajfault)",
                     static_cast<double>(snap.vm.pgmajfault), {{"type", "major"}});
    registry_.set_counter("lpt_swap_pages_total", "Pages swapped in (pswpin)",
                     static_cast<double>(snap.vm.pswpin), {{"direction", "in"}});
    registry_.set_counter("lpt_swap_pages_total", "Pages swapped out (pswpout)",
                     static_cast<double>(snap.vm.pswpout), {{"direction", "out"}});
    registry_.set_counter("lpt_paging_kB_total", "kB paged in from disk (pgpgin)",
                     static_cast<double>(snap.vm.pgpgin), {{"direction", "in"}});
    registry_.set_counter("lpt_paging_kB_total", "kB paged out to disk (pgpgout)",
                     static_cast<double>(snap.vm.pgpgout), {{"direction", "out"}});
    registry_.set_gauge("lpt_vm_nr_dirty", "Dirty pages in the page cache (/proc/vmstat nr_dirty)",
                   static_cast<double>(snap.vm.nr_dirty));

    for (const auto& d : snap.disk.devices) {
        Labels dev{{"device", d.name}};
        registry_.set_counter("lpt_disk_reads_completed_total",
                         "Completed reads from /proc/diskstats (block layer)",
                         static_cast<double>(d.reads_completed), dev);
        registry_.set_counter("lpt_disk_writes_completed_total",
                         "Completed writes from /proc/diskstats (block layer)",
                         static_cast<double>(d.writes_completed), dev);
        registry_.set_counter("lpt_disk_read_bytes_total", "Bytes read assuming 512-byte sectors",
                         static_cast<double>(d.sectors_read) * 512.0, dev);
        registry_.set_counter("lpt_disk_written_bytes_total", "Bytes written assuming 512-byte sectors",
                         static_cast<double>(d.sectors_written) * 512.0, dev);
        registry_.set_counter("lpt_disk_io_time_seconds_total", "Milliseconds spent doing I/O / 1000",
                         static_cast<double>(d.io_time_ms) / 1000.0, dev);
        registry_.set_gauge("lpt_disk_io_in_progress", "In-flight I/O requests",
                       static_cast<double>(d.io_in_progress), dev);
    }

    std::size_t n = 0;
    for (const auto& p : snap.processes.processes) {
        if (process_limit_ > 0 && n >= process_limit_) {
            break;
        }
        ++n;
        Labels l{{"pid", std::to_string(p.pid)}, {"comm", p.comm}};
        registry_.set_counter("lpt_process_cpu_ticks_total",
                         "utime+stime ticks from /proc/<pid>/stat",
                         static_cast<double>(p.utime + p.stime), l);
        registry_.set_gauge("lpt_process_rss_bytes", "Resident set size (rss pages * 4096 estimate)",
                       static_cast<double>(p.rss_pages) * 4096.0, l);
        registry_.set_gauge("lpt_process_vsize_bytes", "Virtual memory size from /proc/<pid>/stat",
                       static_cast<double>(p.vsize), l);
        registry_.set_counter("lpt_process_page_faults_total", "minflt from /proc/<pid>/stat",
                         static_cast<double>(p.minflt),
                         {{"pid", std::to_string(p.pid)}, {"comm", p.comm}, {"type", "minor"}});
        registry_.set_counter("lpt_process_page_faults_total", "majflt from /proc/<pid>/stat",
                         static_cast<double>(p.majflt),
                         {{"pid", std::to_string(p.pid)}, {"comm", p.comm}, {"type", "major"}});
        registry_.set_counter("lpt_process_context_switches_total",
                         "voluntary_ctxt_switches from /proc/<pid>/status",
                         static_cast<double>(p.voluntary_ctxt_switches),
                         {{"pid", std::to_string(p.pid)}, {"comm", p.comm}, {"type", "voluntary"}});
        registry_.set_counter("lpt_process_context_switches_total",
                         "nonvoluntary_ctxt_switches from /proc/<pid>/status",
                         static_cast<double>(p.nonvoluntary_ctxt_switches),
                         {{"pid", std::to_string(p.pid)},
                          {"comm", p.comm},
                          {"type", "nonvoluntary"}});
        registry_.set_counter("lpt_process_io_bytes_total", "read_bytes from /proc/<pid>/io",
                         static_cast<double>(p.read_bytes),
                         {{"pid", std::to_string(p.pid)}, {"comm", p.comm}, {"op", "read"}});
        registry_.set_counter("lpt_process_io_bytes_total", "write_bytes from /proc/<pid>/io",
                         static_cast<double>(p.write_bytes),
                         {{"pid", std::to_string(p.pid)}, {"comm", p.comm}, {"op", "write"}});
    }

    prev_cpu_ = snap.cpu;
    have_prev_cpu_ = true;
}

std::string SnapshotExporter::render() const { return registry_.render(); }

}  // namespace lpt
