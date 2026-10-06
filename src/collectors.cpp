#include "lpt/collectors.hpp"
#include "lpt/string_util.hpp"

#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <stdexcept>

namespace lpt {
namespace {

std::uint64_t to_u64(const std::string& s, std::uint64_t fallback = 0) {
    if (s.empty()) {
        return fallback;
    }
    char* end = nullptr;
    const unsigned long long v = std::strtoull(s.c_str(), &end, 10);
    if (end == s.c_str()) {
        return fallback;
    }
    return static_cast<std::uint64_t>(v);
}



CpuTimes parse_cpu_line(const std::vector<std::string>& f) {
    CpuTimes t;
    if (f.empty()) {
        return t;
    }
    t.cpu_id = f[0];
    auto at = [&](std::size_t i) { return i < f.size() ? to_u64(f[i]) : 0; };
    t.user = at(1);
    t.nice = at(2);
    t.system = at(3);
    t.idle = at(4);
    t.iowait = at(5);
    t.irq = at(6);
    t.softirq = at(7);
    t.steal = at(8);
    t.guest = at(9);
    t.guest_nice = at(10);
    return t;
}

bool is_whole_disk(const std::string& name) {
    // Skip partitions (sda1, nvme0n1p1) and virtual ram/loop devices by default.
    if (starts_with(name, "loop") || starts_with(name, "ram") || starts_with(name, "zram")) {
        return false;
    }
    if (starts_with(name, "dm-")) {
        return true;
    }
    if (starts_with(name, "nvme")) {
        return name.find('p') == std::string::npos;
    }
    if (starts_with(name, "mmcblk")) {
        return name.find('p') == std::string::npos;
    }
    // sda, vda, xvda: letters only after the prefix type.
    const auto last = name.find_last_not_of("0123456789");
    return last != std::string::npos && last == name.size() - 1;
}

}  // namespace

CpuSnapshot parse_proc_stat(const std::string& text) {
    CpuSnapshot snap;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        const auto f = split_ws(line);
        if (f.empty()) {
            continue;
        }
        if (f[0] == "cpu" || starts_with(f[0], "cpu")) {
            CpuTimes t = parse_cpu_line(f);
            if (f[0] == "cpu") {
                snap.aggregate = t;
            } else {
                snap.per_cpu.push_back(t);
            }
        } else if (f[0] == "ctxt" && f.size() >= 2) {
            snap.context_switches = to_u64(f[1]);
        } else if (f[0] == "processes" && f.size() >= 2) {
            snap.processes_created = to_u64(f[1]);
        } else if (f[0] == "procs_running" && f.size() >= 2) {
            snap.procs_running = to_u64(f[1]);
        } else if (f[0] == "procs_blocked" && f.size() >= 2) {
            snap.procs_blocked = to_u64(f[1]);
        }
    }
    return snap;
}

CpuSnapshot collect_cpu(const ProcFs& fs) {
    return parse_proc_stat(fs.read_file("stat"));
}

MemorySnapshot parse_meminfo(const std::string& text) {
    MemorySnapshot m;
    std::istringstream in(text);
    std::string line;
    auto take = [&](const std::string& key, std::uint64_t& dest) {
        if (starts_with(line, key)) {
            const auto f = split_ws(line);
            if (f.size() >= 2) {
                dest = to_u64(f[1]);
            }
        }
    };
    while (std::getline(in, line)) {
        take("MemTotal:", m.mem_total_kb);
        take("MemFree:", m.mem_free_kb);
        take("MemAvailable:", m.mem_available_kb);
        take("Buffers:", m.buffers_kb);
        take("Cached:", m.cached_kb);
        take("SwapTotal:", m.swap_total_kb);
        take("SwapFree:", m.swap_free_kb);
        take("AnonPages:", m.anon_pages_kb);
        take("Mapped:", m.mapped_kb);
        take("Shmem:", m.shmem_kb);
        take("Dirty:", m.dirty_kb);
        take("Writeback:", m.writeback_kb);
    }
    return m;
}

MemorySnapshot collect_memory(const ProcFs& fs) {
    return parse_meminfo(fs.read_file("meminfo"));
}

VmSnapshot parse_vmstat(const std::string& text) {
    VmSnapshot v;
    std::istringstream in(text);
    std::string key;
    std::uint64_t value = 0;
    while (in >> key >> value) {
        if (key == "pgfault") {
            v.pgfault = value;
        } else if (key == "pgmajfault") {
            v.pgmajfault = value;
        } else if (key == "pswpin") {
            v.pswpin = value;
        } else if (key == "pswpout") {
            v.pswpout = value;
        } else if (key == "pgpgin") {
            v.pgpgin = value;
        } else if (key == "pgpgout") {
            v.pgpgout = value;
        } else if (key == "nr_dirty") {
            v.nr_dirty = value;
        } else if (key == "nr_writeback") {
            v.nr_writeback = value;
        } else if (key == "numa_hit") {
            v.numa_hit = value;
        } else if (key == "numa_miss") {
            v.numa_miss = value;
        }
    }
    return v;
}

VmSnapshot collect_vmstat(const ProcFs& fs) {
    return parse_vmstat(fs.read_file("vmstat"));
}

DiskSnapshot parse_diskstats(const std::string& text, bool skip_ram_and_loop) {
    DiskSnapshot snap;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        const auto f = split_ws(line);
        if (f.size() < 14) {
            continue;
        }
        DiskDevice d;
        d.name = f[2];
        if (skip_ram_and_loop && !is_whole_disk(d.name)) {
            continue;
        }
        d.reads_completed = to_u64(f[3]);
        d.reads_merged = to_u64(f[4]);
        d.sectors_read = to_u64(f[5]);
        d.read_time_ms = to_u64(f[6]);
        d.writes_completed = to_u64(f[7]);
        d.writes_merged = to_u64(f[8]);
        d.sectors_written = to_u64(f[9]);
        d.write_time_ms = to_u64(f[10]);
        d.io_in_progress = to_u64(f[11]);
        d.io_time_ms = to_u64(f[12]);
        d.weighted_io_time_ms = to_u64(f[13]);
        snap.devices.push_back(std::move(d));
    }
    return snap;
}

DiskSnapshot collect_disk(const ProcFs& fs) {
    return parse_diskstats(fs.read_file("diskstats"), true);
}

ProcessSample parse_proc_pid_stat(int pid, const std::string& text) {
    ProcessSample p;
    p.pid = pid;
    const auto open = text.find('(');
    const auto close = text.rfind(')');
    if (open == std::string::npos || close == std::string::npos || close <= open) {
        throw std::runtime_error("malformed /proc/pid/stat");
    }
    p.comm = text.substr(open + 1, close - open - 1);
    
    const char* ptr = text.c_str() + close + 1;
    while (*ptr == ' ' || *ptr == '\t') ptr++;
    
    if (*ptr == '\0') {
        throw std::runtime_error("short /proc/pid/stat");
    }
    
    p.state = *ptr;
    ptr++;
    
    auto next_u64 = [&]() -> std::uint64_t {
        char* end;
        std::uint64_t v = std::strtoull(ptr, &end, 10);
        if (ptr == end) throw std::runtime_error("short /proc/pid/stat");
        ptr = end;
        return v;
    };
    auto next_i64 = [&]() -> std::int64_t {
        char* end;
        std::int64_t v = std::strtoll(ptr, &end, 10);
        if (ptr == end) throw std::runtime_error("short /proc/pid/stat");
        ptr = end;
        return v;
    };

    try {
        for (int i = 0; i < 6; ++i) next_u64();
        p.minflt = next_u64();
        next_u64();
        p.majflt = next_u64();
        next_u64();
        p.utime = next_u64();
        p.stime = next_u64();
        p.cutime = next_u64();
        p.cstime = next_u64();
        next_u64();
        next_i64();
        p.num_threads = next_i64();
        next_u64();
        next_u64();
        p.vsize = next_u64();
        p.rss_pages = next_i64();
    } catch (const std::exception&) {
        throw std::runtime_error("short /proc/pid/stat");
    }
    return p;
}

void apply_status_fields(ProcessSample& sample, const std::string& status_text) {
    std::istringstream in(status_text);
    std::string line;
    while (std::getline(in, line)) {
        if (starts_with(line, "voluntary_ctxt_switches:")) {
            sample.voluntary_ctxt_switches = std::strtoull(line.c_str() + 24, nullptr, 10);
        } else if (starts_with(line, "nonvoluntary_ctxt_switches:")) {
            sample.nonvoluntary_ctxt_switches = std::strtoull(line.c_str() + 27, nullptr, 10);
        }
    }
}

void apply_io_fields(ProcessSample& sample, const std::string& io_text) {
    std::istringstream in(io_text);
    std::string line;
    while (std::getline(in, line)) {
        if (starts_with(line, "read_bytes:")) {
            sample.read_bytes = std::strtoull(line.c_str() + 11, nullptr, 10);
        } else if (starts_with(line, "write_bytes:")) {
            sample.write_bytes = std::strtoull(line.c_str() + 12, nullptr, 10);
        }
    }
}

ProcessSnapshot collect_processes(const ProcFs& fs, std::size_t limit) {
    ProcessSnapshot snap;
    auto pids = fs.list_numeric_dirs("");
    std::sort(pids.begin(), pids.end(), [](const std::string& a, const std::string& b) {
        return std::stoi(a) < std::stoi(b);
    });
    for (const auto& pid_s : pids) {
        if (limit > 0 && snap.processes.size() >= limit) {
            break;
        }
        try {
            const int pid = std::stoi(pid_s);
            ProcessSample s = parse_proc_pid_stat(pid, fs.read_file(pid_s + "/stat"));
            if (fs.exists(pid_s + "/status")) {
                apply_status_fields(s, fs.read_file(pid_s + "/status"));
            }
            if (fs.exists(pid_s + "/io")) {
                apply_io_fields(s, fs.read_file(pid_s + "/io"));
            }
            snap.processes.push_back(std::move(s));
        } catch (const std::exception&) {
            continue;
        }
    }
    return snap;
}

SystemSnapshot collect_system(const ProcFs& fs, std::size_t process_limit) {
    SystemSnapshot s;
    s.cpu = collect_cpu(fs);
    s.memory = collect_memory(fs);
    s.vm = collect_vmstat(fs);
    s.disk = collect_disk(fs);
    s.processes = collect_processes(fs, process_limit);
    return s;
}

}  // namespace lpt
