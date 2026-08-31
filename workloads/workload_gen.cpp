#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>
#include <csignal>

namespace {

std::atomic<bool> g_stop{false};

void on_signal(int) { g_stop = true; }

void usage(const char* a0) {
    std::cerr
        << "Workload generator for linux-perf-telemetry experiments.\n\n"
        << "Usage: " << a0 << " --type {cpu|memory|io} [options]\n"
        << "  --seconds N         Run duration (default 10)\n"
        << "  --threads N         Worker threads (default 1)\n"
        << "  --size-mb N         Memory working set (memory workload, default 256)\n"
        << "  --pattern {seq|rand}  Access pattern (memory, default seq)\n"
        << "  --path FILE         I/O file (default /tmp/lpt-io.bin)\n"
        << "  --block-kb N        I/O block size (default 64)\n"
        << "  --sync              fsync after each write (I/O, default off)\n"
        << "  --optimized         Use the 'after' algorithm (see README experiments)\n";
}

void burn_cpu_naive(std::uint64_t* sink) {
    // Intentionally branchy and memory-churning to show a before/after with --optimized.
    volatile double acc = 0.0;
    while (!g_stop.load(std::memory_order_relaxed)) {
        for (int i = 0; i < 10000; ++i) {
            if ((i & 1) == 0) {
                acc += static_cast<double>(i) * 1.0000001;
            } else {
                acc -= static_cast<double>(i) / 1.0000001;
            }
        }
        *sink += static_cast<std::uint64_t>(acc);
    }
}

void burn_cpu_optimized(std::uint64_t* sink) {
    std::uint64_t acc = 0;
    while (!g_stop.load(std::memory_order_relaxed)) {
        for (std::uint64_t i = 0; i < 100000; ++i) {
            acc += i * 0x9e3779b97f4a7c15ULL;
            acc ^= acc >> 17;
        }
        *sink += acc;
    }
}

void burn_mem(std::size_t bytes, bool random_access, std::uint64_t* sink) {
    std::vector<std::uint64_t> buf(bytes / sizeof(std::uint64_t), 1);
    const std::size_t n = buf.size();
    std::mt19937_64 rng(1);
    std::uint64_t acc = 0;
    if (!random_access) {
        while (!g_stop.load(std::memory_order_relaxed)) {
            for (std::size_t i = 0; i < n; ++i) {
                acc += buf[i];
                buf[i] = acc;
            }
            *sink += acc;
        }
        return;
    }
    std::uniform_int_distribution<std::size_t> dist(0, n - 1);
    while (!g_stop.load(std::memory_order_relaxed)) {
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t idx = dist(rng);
            acc += buf[idx];
            buf[idx] = acc;
        }
        *sink += acc;
    }
}

void burn_io(const std::string& path, std::size_t block, bool do_sync, bool sequential,
             std::uint64_t* sink) {
    const int flags = O_CREAT | O_RDWR | O_TRUNC;
    const int fd = ::open(path.c_str(), flags, 0644);
    if (fd < 0) {
        throw std::runtime_error("open failed: " + path);
    }
    std::vector<char> buf(block, 'A');
    std::uint64_t off = 0;
    const std::uint64_t cap = 64ULL * 1024ULL * 1024ULL;  // wrap a 64 MiB file
    std::mt19937_64 rng(2);
    while (!g_stop.load(std::memory_order_relaxed)) {
        if (!sequential) {
            off = (rng() % (cap / block)) * block;
            if (::lseek(fd, static_cast<off_t>(off), SEEK_SET) < 0) {
                break;
            }
        }
        const ssize_t n = ::write(fd, buf.data(), buf.size());
        if (n < 0) {
            break;
        }
        if (do_sync) {
            ::fsync(fd);
        }
        off += static_cast<std::uint64_t>(n);
        if (sequential && off >= cap) {
            if (::lseek(fd, 0, SEEK_SET) < 0) {
                break;
            }
            off = 0;
        }
        *sink += static_cast<std::uint64_t>(n);
    }
    ::close(fd);
}

}  // namespace

int main(int argc, char** argv) {
    std::string type = "cpu";
    int seconds = 10;
    int threads = 1;
    int size_mb = 256;
    std::string pattern = "rand";
    std::string path = "/tmp/lpt-io.bin";
    int block_kb = 64;
    bool do_sync = false;
    bool optimized = false;

    for (int i = 1; i < argc; ++i) {
        auto need = [&](const char*) { return argv[++i]; };
        if (std::strcmp(argv[i], "--type") == 0) {
            type = need("--type");
        } else if (std::strcmp(argv[i], "--seconds") == 0) {
            seconds = std::atoi(need("--seconds"));
        } else if (std::strcmp(argv[i], "--threads") == 0) {
            threads = std::atoi(need("--threads"));
        } else if (std::strcmp(argv[i], "--size-mb") == 0) {
            size_mb = std::atoi(need("--size-mb"));
        } else if (std::strcmp(argv[i], "--pattern") == 0) {
            pattern = need("--pattern");
        } else if (std::strcmp(argv[i], "--path") == 0) {
            path = need("--path");
        } else if (std::strcmp(argv[i], "--block-kb") == 0) {
            block_kb = std::atoi(need("--block-kb"));
        } else if (std::strcmp(argv[i], "--sync") == 0) {
            do_sync = true;
        } else if (std::strcmp(argv[i], "--optimized") == 0) {
            optimized = true;
        } else if (std::strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    struct sigaction sa {};
    sa.sa_handler = on_signal;
    ::sigaction(SIGINT, &sa, nullptr);
    ::sigaction(SIGTERM, &sa, nullptr);

    std::vector<std::uint64_t> sinks(static_cast<std::size_t>(threads), 0);
    std::vector<std::thread> workers;
    workers.reserve(static_cast<std::size_t>(threads));

    auto spawn = [&](auto fn) {
        for (int t = 0; t < threads; ++t) {
            workers.emplace_back(fn, t);
        }
    };

    if (type == "cpu") {
        spawn([&](int t) {
            if (optimized) {
                burn_cpu_optimized(&sinks[static_cast<std::size_t>(t)]);
            } else {
                burn_cpu_naive(&sinks[static_cast<std::size_t>(t)]);
            }
        });
    } else if (type == "memory") {
        spawn([&](int t) {
            // Default/before: random access (cache/TLB misses). --optimized: sequential stream
            // unless --pattern rand is set.
            const bool random_access = optimized ? false : (pattern != "seq");
            burn_mem(static_cast<std::size_t>(size_mb) * 1024ULL * 1024ULL, random_access,
                     &sinks[static_cast<std::size_t>(t)]);
        });
    } else if (type == "io") {
        spawn([&](int t) {
            std::string p = path;
            if (threads > 1) {
                p += "." + std::to_string(t);
            }
            const bool sequential = optimized;
            burn_io(p, static_cast<std::size_t>(block_kb) * 1024ULL, do_sync, sequential,
                    &sinks[static_cast<std::size_t>(t)]);
        });
    } else {
        usage(argv[0]);
        return 2;
    }

    std::cerr << "workload type=" << type << " threads=" << threads << " seconds=" << seconds
              << " optimized=" << (optimized ? "yes" : "no") << " pid=" << ::getpid() << "\n";
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    while (!g_stop.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    g_stop = true;
    for (auto& w : workers) {
        w.join();
    }
    std::uint64_t sum = 0;
    for (auto s : sinks) {
        sum += s;
    }
    std::cerr << "sink=" << sum << "\n";
    return 0;
}
