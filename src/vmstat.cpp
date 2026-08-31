#include "lpt/vmstat.hpp"

#include <sstream>

namespace lpt {

VmstatCounters parse_vmstat(const std::string& text) {
    VmstatCounters c;
    std::istringstream ss(text);
    std::string key;
    std::uint64_t val = 0;
    while (ss >> key >> val) {
        if (key == "pgfault") {
            c.pgfault = val;
        } else if (key == "pgmajfault") {
            c.pgmajfault = val;
        } else if (key == "pgpgin") {
            c.pgpgin = val;
        } else if (key == "pgpgout") {
            c.pgpgout = val;
        } else if (key == "pswpin") {
            c.pswpin = val;
        } else if (key == "pswpout") {
            c.pswpout = val;
        } else if (key == "nr_dirty") {
            c.nr_dirty = val;
        } else if (key == "nr_writeback") {
            c.nr_writeback = val;
        } else if (key == "nr_free_pages") {
            c.nr_free_pages = val;
        }
    }
    return c;
}

VmstatCounters collect_vmstat(const ProcReader& reader) {
    return parse_vmstat(reader.read_file("/proc/vmstat"));
}

VmstatRates compute_vmstat_rates(const VmstatCounters& prev, const VmstatCounters& cur,
                                 double elapsed_sec) {
    const double sec = elapsed_sec > 0 ? elapsed_sec : 1.0;
    VmstatRates r;
    const auto dpgfault = cur.pgfault - prev.pgfault;
    const auto dmaj = cur.pgmajfault - prev.pgmajfault;
    r.major_faults_per_sec = static_cast<double>(dmaj) / sec;
    r.minor_faults_per_sec = static_cast<double>(dpgfault - dmaj) / sec;
    r.pages_in_per_sec = static_cast<double>(cur.pgpgin - prev.pgpgin) / sec;
    r.pages_out_per_sec = static_cast<double>(cur.pgpgout - prev.pgpgout) / sec;
    r.swap_in_per_sec = static_cast<double>(cur.pswpin - prev.pswpin) / sec;
    r.swap_out_per_sec = static_cast<double>(cur.pswpout - prev.pswpout) / sec;
    return r;
}

}  // namespace lpt
