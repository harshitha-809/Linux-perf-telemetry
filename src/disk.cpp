#include "lpt/disk.hpp"

#include <algorithm>
#include <sstream>
#include <unordered_map>

namespace lpt {
namespace {

bool is_partition(const std::string& name) {
    // Skip typical partitions (sda1, nvme0n1p1) so we report whole devices.
    if (name.size() >= 2) {
        const char last = name.back();
        if (last >= '0' && last <= '9') {
            if (name.find("nvme") != std::string::npos) {
                return name.find('p') != std::string::npos;
            }
            if (name.find("loop") != std::string::npos || name.find("ram") != std::string::npos) {
                return false;
            }
            return true;
        }
    }
    return false;
}

}  // namespace

std::vector<DiskCounters> parse_diskstats(const std::string& text) {
    std::vector<DiskCounters> out;
    std::istringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) {
        auto tok = ProcReader::split_ws(line);
        // major minor name rio rmerge rsect ruse wio wmerge wsect wuse running use weighted
        if (tok.size() < 14) {
            continue;
        }
        DiskCounters d;
        d.name = tok[2];
        if (is_partition(d.name)) {
            continue;
        }
        if (d.name.rfind("loop", 0) == 0 || d.name.rfind("ram", 0) == 0) {
            continue;
        }
        auto u = [&](std::size_t i) {
            return static_cast<std::uint64_t>(ProcReader::parse_i64(tok[i]));
        };
        d.reads_completed = u(3);
        d.reads_merged = u(4);
        d.sectors_read = u(5);
        d.ms_reading = u(6);
        d.writes_completed = u(7);
        d.writes_merged = u(8);
        d.sectors_written = u(9);
        d.ms_writing = u(10);
        d.ios_in_progress = u(11);
        d.ms_io = u(12);
        d.weighted_ms_io = u(13);
        out.push_back(d);
    }
    return out;
}

std::vector<DiskCounters> collect_disk(const ProcReader& reader) {
    return parse_diskstats(reader.read_file("/proc/diskstats"));
}

std::vector<DiskRates> compute_disk_rates(const std::vector<DiskCounters>& prev,
                                          const std::vector<DiskCounters>& cur,
                                          double elapsed_sec) {
    std::unordered_map<std::string, DiskCounters> pmap;
    pmap.reserve(prev.size());
    for (const auto& d : prev) {
        pmap[d.name] = d;
    }
    const double sec = elapsed_sec > 0 ? elapsed_sec : 1.0;
    std::vector<DiskRates> rates;
    rates.reserve(cur.size());
    for (const auto& c : cur) {
        auto it = pmap.find(c.name);
        if (it == pmap.end()) {
            continue;
        }
        const auto& p = it->second;
        DiskRates r;
        r.name = c.name;
        r.reads_per_sec = static_cast<double>(c.reads_completed - p.reads_completed) / sec;
        r.writes_per_sec = static_cast<double>(c.writes_completed - p.writes_completed) / sec;
        // Linux diskstats sectors are 512 bytes.
        r.read_bytes_per_sec =
            static_cast<double>(c.sectors_read - p.sectors_read) * 512.0 / sec;
        r.write_bytes_per_sec =
            static_cast<double>(c.sectors_written - p.sectors_written) * 512.0 / sec;
        const double dms = static_cast<double>(c.ms_io - p.ms_io);
        r.util_ratio = std::min(1.0, (dms / (sec * 1000.0)));
        r.avg_queue_depth = static_cast<double>(c.weighted_ms_io - p.weighted_ms_io) / (sec * 1000.0);
        rates.push_back(r);
    }
    return rates;
}

}  // namespace lpt
