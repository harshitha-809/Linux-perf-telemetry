#include "lpt/process.hpp"

#include <algorithm>
#include <unordered_map>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace lpt {
namespace {

long sys_clk_tck() {
#ifdef _WIN32
    return 100;
#else
    long v = ::sysconf(_SC_CLK_TCK);
    return v > 0 ? v : 100;
#endif
}

long sys_page_size() {
#ifdef _WIN32
    return 4096;
#else
    long v = ::sysconf(_SC_PAGESIZE);
    return v > 0 ? v : 4096;
#endif
}

// /proc/pid/stat: comm is in parentheses and may contain spaces.
bool split_stat(const std::string& text, int& pid, std::string& comm, std::vector<std::string>& rest) {
    const auto open = text.find('(');
    const auto close = text.rfind(')');
    if (open == std::string::npos || close == std::string::npos || close < open) {
        return false;
    }
    pid = static_cast<int>(ProcReader::parse_i64(text.substr(0, open)));
    comm = text.substr(open + 1, close - open - 1);
    rest = ProcReader::split_ws(text.substr(close + 1));
    return !rest.empty();
}

}  // namespace

ProcessSample parse_proc_pid_stat(int pid, const std::string& stat_text) {
    ProcessSample s;
    s.pid = pid;
    std::string comm;
    std::vector<std::string> f;
    int parsed_pid = pid;
    if (!split_stat(stat_text, parsed_pid, comm, f)) {
        return s;
    }
    s.pid = parsed_pid != 0 ? parsed_pid : pid;
    s.comm = std::move(comm);
    // fields after comm: state=0, ppid=1, ... utime=11, stime=12, cutime=13, cstime=14
    // minflt=7, cminflt=8, majflt=9, cmajflt=10, vsize=20, rss=21
    if (f.size() > 0 && !f[0].empty()) {
        s.state = f[0][0];
    }
    auto u = [&](std::size_t i) -> std::uint64_t {
        return i < f.size() ? static_cast<std::uint64_t>(ProcReader::parse_i64(f[i])) : 0;
    };
    auto i64 = [&](std::size_t i) -> std::int64_t {
        return i < f.size() ? ProcReader::parse_i64(f[i]) : 0;
    };
    s.minflt = u(7);
    s.majflt = u(9);
    s.utime = u(11);
    s.stime = u(12);
    s.cutime = i64(13);
    s.cstime = i64(14);
    s.vsize = u(20);
    s.rss_pages = i64(21);
    return s;
}

void enrich_from_status(ProcessSample& sample, const std::string& status_text) {
    const auto kv = ProcReader::parse_key_value_kb(status_text);
    auto get = [&](const char* k) -> std::int64_t {
        auto it = kv.find(k);
        return it == kv.end() ? 0 : it->second;
    };
    sample.threads = static_cast<int>(get("Threads"));
    if (sample.threads <= 0) {
        sample.threads = 1;
    }
    sample.voluntary_ctxt = static_cast<std::uint64_t>(std::max<std::int64_t>(0, get("voluntary_ctxt_switches")));
    sample.nonvoluntary_ctxt =
        static_cast<std::uint64_t>(std::max<std::int64_t>(0, get("nonvoluntary_ctxt_switches")));
    sample.nvcsw = sample.voluntary_ctxt;
    sample.nivcsw = sample.nonvoluntary_ctxt;
}

void enrich_from_io(ProcessSample& sample, const std::string& io_text) {
    const auto kv = ProcReader::parse_key_value_kb(io_text);
    auto get = [&](const char* k) -> std::uint64_t {
        auto it = kv.find(k);
        if (it == kv.end() || it->second < 0) {
            return 0;
        }
        return static_cast<std::uint64_t>(it->second);
    };
    sample.read_bytes = get("read_bytes");
    sample.write_bytes = get("write_bytes");
}

std::vector<ProcessSample> collect_processes(const ProcReader& reader, std::size_t max_procs) {
    std::vector<ProcessSample> out;
    std::vector<int> pids;
    try {
        pids = reader.list_pids();
    } catch (...) {
        return out;
    }
    out.reserve(std::min(max_procs, pids.size()));
    for (int pid : pids) {
        if (out.size() >= max_procs) {
            break;
        }
        const std::string base = "/proc/" + std::to_string(pid);
        try {
            auto s = parse_proc_pid_stat(pid, reader.read_file(base + "/stat"));
            try {
                enrich_from_status(s, reader.read_file(base + "/status"));
            } catch (...) {
            }
            try {
                enrich_from_io(s, reader.read_file(base + "/io"));
            } catch (...) {
            }
            out.push_back(std::move(s));
        } catch (...) {
            continue;
        }
    }
    return out;
}

std::vector<ProcessRates> compute_process_rates(const std::vector<ProcessSample>& prev,
                                                const std::vector<ProcessSample>& cur,
                                                double elapsed_sec, long clk_tck,
                                                long page_size) {
    if (clk_tck <= 0) {
        clk_tck = sys_clk_tck();
    }
    if (page_size <= 0) {
        page_size = sys_page_size();
    }
    std::unordered_map<int, ProcessSample> pmap;
    pmap.reserve(prev.size());
    for (const auto& p : prev) {
        pmap[p.pid] = p;
    }
    const double sec = elapsed_sec > 0 ? elapsed_sec : 1.0;
    std::vector<ProcessRates> rates;
    rates.reserve(cur.size());
    for (const auto& c : cur) {
        auto it = pmap.find(c.pid);
        if (it == pmap.end()) {
            continue;
        }
        const auto& p = it->second;
        ProcessRates r;
        r.pid = c.pid;
        r.comm = c.comm;
        r.state = c.state;
        r.threads = c.threads;
        r.rss_bytes = static_cast<std::uint64_t>(std::max<std::int64_t>(0, c.rss_pages)) *
                      static_cast<std::uint64_t>(page_size);
        r.vsize_bytes = c.vsize;
        const double ticks =
            static_cast<double>((c.utime + c.stime) - (p.utime + p.stime));
        r.cpu_ratio = (ticks / static_cast<double>(clk_tck)) / sec;
        r.minor_faults_per_sec = static_cast<double>(c.minflt - p.minflt) / sec;
        r.major_faults_per_sec = static_cast<double>(c.majflt - p.majflt) / sec;
        r.read_bytes_per_sec = static_cast<double>(c.read_bytes - p.read_bytes) / sec;
        r.write_bytes_per_sec = static_cast<double>(c.write_bytes - p.write_bytes) / sec;
        r.voluntary_csw_per_sec = static_cast<double>(c.nvcsw - p.nvcsw) / sec;
        r.involuntary_csw_per_sec = static_cast<double>(c.nivcsw - p.nivcsw) / sec;
        rates.push_back(std::move(r));
    }
    std::sort(rates.begin(), rates.end(), [](const ProcessRates& a, const ProcessRates& b) {
        return a.cpu_ratio > b.cpu_ratio;
    });
    return rates;
}

}  // namespace lpt
