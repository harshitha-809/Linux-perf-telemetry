#include "lpt/workload.hpp"

#include <atomic>
#include <csignal>
#include <cstring>
#include <iostream>
#include <string>

namespace {
std::atomic<bool> g_stop{false};
void on_signal(int) { g_stop.store(true); }

void usage(const char* argv0) {
    std::cerr
        << "Usage: " << argv0
        << " <cpu|memory|io> [--mode naive|optimized] [--seconds N] [--threads N]\n"
        << "                 [--size-mb N] [--io-path PATH]\n";
}
}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        usage(argv[0]);
        return 2;
    }

    lpt::WorkloadConfig cfg;
    const std::string kind = argv[1];
    if (kind == "cpu") {
        cfg.kind = lpt::WorkloadKind::Cpu;
    } else if (kind == "memory") {
        cfg.kind = lpt::WorkloadKind::Memory;
    } else if (kind == "io") {
        cfg.kind = lpt::WorkloadKind::Io;
    } else if (kind == "-h" || kind == "--help") {
        usage(argv[0]);
        return 0;
    } else {
        usage(argv[0]);
        return 2;
    }

    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            const std::string m = argv[++i];
            cfg.mode = (m == "optimized") ? lpt::WorkloadMode::Optimized : lpt::WorkloadMode::Naive;
        } else if (std::strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) {
            cfg.seconds = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
            cfg.threads = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--size-mb") == 0 && i + 1 < argc) {
            cfg.size_mb = static_cast<std::uint64_t>(std::atoll(argv[++i]));
        } else if (std::strcmp(argv[i], "--io-path") == 0 && i + 1 < argc) {
            cfg.io_path = argv[++i];
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);
    std::cerr << "lpt-workload kind=" << kind
              << " mode=" << (cfg.mode == lpt::WorkloadMode::Optimized ? "optimized" : "naive")
              << " seconds=" << cfg.seconds << " threads=" << cfg.threads << '\n';
    return lpt::run_workload(cfg, g_stop);
}
