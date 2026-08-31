#include "lpt/collector.hpp"
#include "lpt/http_server.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>

namespace {

std::atomic<bool> g_stop{false};

void on_signal(int) { g_stop = true; }

void usage(const char* argv0) {
    std::cerr
        << "Usage: " << argv0 << " [options]\n"
        << "  --listen ADDR     Bind address (default 0.0.0.0)\n"
        << "  --port PORT       HTTP port (default 9100)\n"
        << "  --interval MS     Scrape interval in milliseconds (default 1000)\n"
        << "  --proc-root PATH  Alternate root for /proc (default /)\n"
        << "  --max-procs N     Process samples to export (default 64)\n"
        << "  --once            Scrape twice and print /metrics then exit\n"
        << "  --help\n";
}

}  // namespace

int main(int argc, char** argv) {
    std::string listen = "0.0.0.0";
    std::uint16_t port = 9100;
    int interval_ms = 1000;
    lpt::CollectorConfig cfg;
    bool once = false;

    for (int i = 1; i < argc; ++i) {
        auto need = [&](const char* flag) -> const char* {
            if (i + 1 >= argc) {
                std::cerr << flag << " requires an argument\n";
                std::exit(2);
            }
            return argv[++i];
        };
        if (std::strcmp(argv[i], "--listen") == 0) {
            listen = need("--listen");
        } else if (std::strcmp(argv[i], "--port") == 0) {
            port = static_cast<std::uint16_t>(std::atoi(need("--port")));
        } else if (std::strcmp(argv[i], "--interval") == 0) {
            interval_ms = std::atoi(need("--interval"));
        } else if (std::strcmp(argv[i], "--proc-root") == 0) {
            cfg.proc_root = need("--proc-root");
        } else if (std::strcmp(argv[i], "--max-procs") == 0) {
            cfg.max_processes = static_cast<std::size_t>(std::atoi(need("--max-procs")));
        } else if (std::strcmp(argv[i], "--once") == 0) {
            once = true;
        } else if (std::strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    lpt::Collector collector(cfg);
    if (once) {
        collector.scrape();
        std::this_thread::sleep_for(std::chrono::milliseconds(std::max(interval_ms, 50)));
        collector.scrape();
        std::cout << collector.prometheus_text();
        return 0;
    }

    struct sigaction sa {};
    sa.sa_handler = on_signal;
    ::sigaction(SIGINT, &sa, nullptr);
    ::sigaction(SIGTERM, &sa, nullptr);

    collector.scrape();
    lpt::HttpServer server(listen, port, [&] { return collector.prometheus_text(); });
    server.start();
    std::cerr << "lpt-exporter listening on http://" << listen << ":" << server.port()
              << "/metrics (pid " << ::getpid() << ")\n";

    while (!g_stop.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
        try {
            collector.scrape();
        } catch (const std::exception& ex) {
            std::cerr << "scrape error: " << ex.what() << "\n";
        }
    }
    server.stop();
    return 0;
}
