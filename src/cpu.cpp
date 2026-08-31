#include "lpt/cpu.hpp"

#include <algorithm>
#include <sstream>

namespace lpt {
namespace {

CpuTimes parse_cpu_line(const std::vector<std::string>& tok) {
    CpuTimes t;
    t.name = tok[0];
    auto at = [&](std::size_t i) -> std::uint64_t {
        return i < tok.size() ? static_cast<std::uint64_t>(ProcReader::parse_i64(tok[i])) : 0;
    };
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

double ratio(std::uint64_t num, std::uint64_t den) {
    if (den == 0) {
        return 0.0;
    }
    return static_cast<double>(num) / static_cast<double>(den);
}

}  // namespace

CpuSnapshot parse_proc_stat(const std::string& text) {
    CpuSnapshot snap;
    std::istringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        auto tok = ProcReader::split_ws(line);
        if (tok.empty()) {
            continue;
        }
        if (tok[0] == "cpu") {
            snap.aggregate = parse_cpu_line(tok);
        } else if (tok[0].rfind("cpu", 0) == 0) {
            snap.per_cpu.push_back(parse_cpu_line(tok));
        } else if (tok[0] == "ctxt" && tok.size() >= 2) {
            snap.context_switches = static_cast<std::uint64_t>(ProcReader::parse_i64(tok[1]));
        } else if (tok[0] == "processes" && tok.size() >= 2) {
            snap.processes_forked = static_cast<std::uint64_t>(ProcReader::parse_i64(tok[1]));
        } else if (tok[0] == "procs_running" && tok.size() >= 2) {
            snap.procs_running = static_cast<std::uint64_t>(ProcReader::parse_i64(tok[1]));
        } else if (tok[0] == "procs_blocked" && tok.size() >= 2) {
            snap.procs_blocked = static_cast<std::uint64_t>(ProcReader::parse_i64(tok[1]));
        } else if (tok[0] == "intr" && tok.size() >= 2) {
            snap.interrupts = static_cast<std::uint64_t>(ProcReader::parse_i64(tok[1]));
        }
    }
    return snap;
}

CpuSnapshot collect_cpu(const ProcReader& reader) {
    return parse_proc_stat(reader.read_file("/proc/stat"));
}

CpuRates compute_cpu_rates(const CpuSnapshot& prev, const CpuSnapshot& cur, double elapsed_sec) {
    CpuRates r;
    const auto dt = cur.aggregate.total() - prev.aggregate.total();
    const auto dbusy = cur.aggregate.busy() - prev.aggregate.busy();
    r.usage_ratio = ratio(dbusy, dt);
    r.user_ratio = ratio(cur.aggregate.user - prev.aggregate.user, dt);
    r.system_ratio = ratio(cur.aggregate.system - prev.aggregate.system, dt);
    r.iowait_ratio = ratio(cur.aggregate.iowait - prev.aggregate.iowait, dt);
    r.irq_ratio = ratio((cur.aggregate.irq + cur.aggregate.softirq) -
                            (prev.aggregate.irq + prev.aggregate.softirq),
                        dt);
    r.steal_ratio = ratio(cur.aggregate.steal - prev.aggregate.steal, dt);

    const double sec = elapsed_sec > 0 ? elapsed_sec : 1.0;
    r.context_switches_per_sec =
        static_cast<double>(cur.context_switches - prev.context_switches) / sec;
    r.forks_per_sec = static_cast<double>(cur.processes_forked - prev.processes_forked) / sec;
    r.interrupts_per_sec = static_cast<double>(cur.interrupts - prev.interrupts) / sec;

    const std::size_t n = std::min(prev.per_cpu.size(), cur.per_cpu.size());
    r.per_cpu_usage.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        const auto pdt = cur.per_cpu[i].total() - prev.per_cpu[i].total();
        const auto pbusy = cur.per_cpu[i].busy() - prev.per_cpu[i].busy();
        r.per_cpu_usage[i] = ratio(pbusy, pdt);
    }
    return r;
}

}  // namespace lpt
