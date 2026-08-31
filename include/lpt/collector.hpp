#pragma once

#include "lpt/metrics_registry.hpp"
#include "lpt/proc_reader.hpp"
#include "lpt/types.hpp"

#include <chrono>
#include <mutex>
#include <string>
#include <vector>

namespace lpt {

struct CollectorConfig {
    std::string proc_root = "/";
    std::size_t max_processes = 64;
    bool collect_processes = true;
};

class Collector {
public:
    explicit Collector(CollectorConfig cfg = {});

    // Sample /proc, compute rates vs previous sample, update registry.
    void scrape();
    std::string prometheus_text() const;
    MetricsRegistry& registry() { return registry_; }

    const CpuRates& last_cpu() const { return last_cpu_; }
    const MemoryInfo& last_memory() const { return last_memory_; }
    const VmstatRates& last_vmstat() const { return last_vmstat_; }
    const std::vector<DiskRates>& last_disk() const { return last_disk_; }
    const std::vector<ProcessRates>& last_procs() const { return last_procs_; }

private:
    void publish_locked();

    CollectorConfig cfg_;
    ProcReader reader_;
    MetricsRegistry registry_;

    bool have_prev_ = false;
    CpuSnapshot prev_cpu_{};
    VmstatCounters prev_vm_{};
    std::vector<DiskCounters> prev_disk_{};
    std::vector<ProcessSample> prev_proc_{};
    std::chrono::steady_clock::time_point prev_tp_{};

    CpuRates last_cpu_{};
    MemoryInfo last_memory_{};
    VmstatRates last_vmstat_{};
    std::vector<DiskRates> last_disk_{};
    std::vector<ProcessRates> last_procs_{};

    mutable std::mutex mu_;
};

}  // namespace lpt
