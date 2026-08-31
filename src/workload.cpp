#include "lpt/workload.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <numeric>
#include <random>
#include <thread>
#include <vector>

namespace lpt {
namespace {

void cpu_naive(std::atomic<bool>& stop) {
    volatile std::uint64_t acc = 1;
    while (!stop.load(std::memory_order_relaxed)) {
        // Serial dependency chain: one add per cycle, poor IPC.
        acc = acc * 1664525u + 1013904223u;
        acc ^= (acc << 13);
        acc ^= (acc >> 7);
        acc ^= (acc << 17);
    }
    (void)acc;
}

void cpu_optimized(std::atomic<bool>& stop) {
    constexpr std::size_t n = 1 << 16;
    alignas(64) std::vector<double> a(n, 1.001);
    alignas(64) std::vector<double> b(n, 1.002);
    double sink = 0.0;
    while (!stop.load(std::memory_order_relaxed)) {
        double sum = 0.0;
        // Contiguous, independent FMAs — auto-vectorizable, high IPC.
        for (std::size_t i = 0; i < n; ++i) {
            sum += a[i] * b[i];
        }
        sink += sum;
    }
    volatile double keep = sink;
    (void)keep;
}

void memory_naive(std::uint64_t size_mb, std::atomic<bool>& stop) {
    const std::size_t n = static_cast<std::size_t>(size_mb) * 1024u * 1024u / sizeof(std::uint64_t);
    std::vector<std::uint64_t> data(n);
    std::iota(data.begin(), data.end(), 1);
    std::vector<std::size_t> idx(n);
    std::iota(idx.begin(), idx.end(), 0);
    std::mt19937_64 rng(1);
    std::shuffle(idx.begin(), idx.end(), rng);
    volatile std::uint64_t acc = 0;
    while (!stop.load(std::memory_order_relaxed)) {
        std::uint64_t local = 0;
        for (std::size_t i = 0; i < n; ++i) {
            local += data[idx[i]];
        }
        acc += local;
    }
    (void)acc;
}

void memory_optimized(std::uint64_t size_mb, std::atomic<bool>& stop) {
    const std::size_t n = static_cast<std::size_t>(size_mb) * 1024u * 1024u / sizeof(std::uint64_t);
    std::vector<std::uint64_t> data(n);
    std::iota(data.begin(), data.end(), 1);
    volatile std::uint64_t acc = 0;
    while (!stop.load(std::memory_order_relaxed)) {
        std::uint64_t local = 0;
        for (std::size_t i = 0; i < n; ++i) {
            local += data[i];
        }
        acc += local;
    }
    (void)acc;
}

void io_naive(const std::string& path, std::atomic<bool>& stop) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    char buf[64];
    std::memset(buf, 'N', sizeof(buf));
    while (!stop.load(std::memory_order_relaxed)) {
        out.write(buf, sizeof(buf));
        out.flush();
    }
}

void io_optimized(const std::string& path, std::atomic<bool>& stop) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    std::vector<char> buf(1 << 20, 'O');
    while (!stop.load(std::memory_order_relaxed)) {
        out.write(buf.data(), static_cast<std::streamsize>(buf.size()));
    }
    out.flush();
}

}  // namespace

int run_workload(const WorkloadConfig& cfg, std::atomic<bool>& stop) {
    std::vector<std::thread> workers;
    workers.reserve(static_cast<std::size_t>(cfg.threads));
    for (int t = 0; t < cfg.threads; ++t) {
        workers.emplace_back([&, t] {
            if (cfg.kind == WorkloadKind::Cpu) {
                if (cfg.mode == WorkloadMode::Naive) {
                    cpu_naive(stop);
                } else {
                    cpu_optimized(stop);
                }
            } else if (cfg.kind == WorkloadKind::Memory) {
                if (cfg.mode == WorkloadMode::Naive) {
                    memory_naive(cfg.size_mb, stop);
                } else {
                    memory_optimized(cfg.size_mb, stop);
                }
            } else {
                std::string path = cfg.io_path;
                if (cfg.threads > 1) {
                    path += "." + std::to_string(t);
                }
                if (cfg.mode == WorkloadMode::Naive) {
                    io_naive(path, stop);
                } else {
                    io_optimized(path, stop);
                }
            }
        });
    }

    if (cfg.seconds > 0) {
        std::this_thread::sleep_for(std::chrono::seconds(cfg.seconds));
        stop.store(true);
    }
    for (auto& w : workers) {
        w.join();
    }
    return 0;
}

}  // namespace lpt
