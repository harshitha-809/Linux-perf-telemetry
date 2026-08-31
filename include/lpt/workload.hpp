#pragma once

#include <atomic>
#include <cstdint>
#include <string>

namespace lpt {

enum class WorkloadKind { Cpu, Memory, Io };
enum class WorkloadMode { Naive, Optimized };

struct WorkloadConfig {
    WorkloadKind kind = WorkloadKind::Cpu;
    WorkloadMode mode = WorkloadMode::Naive;
    int seconds = 10;
    int threads = 1;
    std::uint64_t size_mb = 256;
    std::string io_path = "/tmp/lpt-workload.dat";
};

int run_workload(const WorkloadConfig& cfg, std::atomic<bool>& stop);

}  // namespace lpt
