#include "lpt/collectors.hpp"
#include "lpt/exporter.hpp"
#include "lpt/http_server.hpp"
#include "lpt/proc_fs.hpp"

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <atomic>

namespace {

std::atomic<bool> g_stop{false};

void on_signal(int) { g_stop.store(true); }

void usage(const char* argv0) {
    std::cerr << "Usage: " << argv0
              << " [--listen 0.0.0.0] [--port 9100] [--interval-ms 1000]\n"
              << "       [--proc-root /proc] [--process-limit 64]\n";
}

}  // namespace

int main(int argc, char** argv) {
    std::string listen = "0.0.0.0";
    int port = 9100;
    int interval_ms = 1000;
    std::string proc_root = "/proc";
    std::size_t process_limit = 64;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--listen") == 0 && i + 1 < argc) {
            listen = argv[++i];
        } else if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--interval-ms") == 0 && i + 1 < argc) {
            interval_ms = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--proc-root") == 0 && i + 1 < argc) {
            proc_root = argv[++i];
        } else if (std::strcmp(argv[i], "--process-limit") == 0 && i + 1 < argc) {
            process_limit = static_cast<std::size_t>(std::atoi(argv[++i]));
        } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    lpt::ProcFs fs(proc_root);
    lpt::SnapshotExporter exporter(process_limit);

    auto scrape = [&] {
        try {
            exporter.update(lpt::collect_system(fs, process_limit));
        } catch (const std::exception& ex) {
            std::cerr << "scrape error: " << ex.what() << '\n';
        }
    };
    scrape();

    lpt::HttpServer server(listen, static_cast<std::uint16_t>(port), [&] { return exporter.render(); });
    if (!server.start()) {
        std::cerr << "Failed to bind HTTP server on " << listen << ':' << port
                  << " (Linux sockets required).\nDumping one scrape to stdout:\n";
        std::cout << exporter.render();
        return 1;
    }

    std::cerr << "lpt-agent listening on http://" << listen << ':' << port << "/metrics\n";
    while (!g_stop.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
        scrape();
    }
    server.stop();
    return 0;
}
