#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <thread>

namespace lpt {

class HttpServer {
public:
    using Handler = std::function<std::string()>;

    HttpServer(std::string bind_address, std::uint16_t port, Handler metrics);
    ~HttpServer();

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    bool start();
    void stop();
    bool running() const { return running_; }
    std::uint16_t port() const { return port_; }

private:
    void loop();

    std::string bind_address_;
    std::uint16_t port_;
    Handler metrics_;
    int listen_fd_ = -1;
    bool running_ = false;
    bool stop_ = false;
    std::thread worker_;
};

}  // namespace lpt
