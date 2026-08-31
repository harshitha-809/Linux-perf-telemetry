#include "lpt/http_server.hpp"

#include <cstring>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#if defined(__linux__)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace lpt {
namespace {

std::string http_response(int code, const char* status, const std::string& body,
                          const char* content_type) {
    std::ostringstream os;
    os << "HTTP/1.1 " << code << ' ' << status << "\r\n"
       << "Content-Type: " << content_type << "\r\n"
       << "Content-Length: " << body.size() << "\r\n"
       << "Connection: close\r\n\r\n"
       << body;
    return os.str();
}

}  // namespace

HttpServer::HttpServer(std::string bind_address, std::uint16_t port, Handler metrics)
    : bind_address_(std::move(bind_address)), port_(port), metrics_(std::move(metrics)) {}

HttpServer::~HttpServer() { stop(); }

bool HttpServer::start() {
#if !defined(__linux__)
    return false;
#else
    listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        return false;
    }
    int yes = 1;
    ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    if (::inet_pton(AF_INET, bind_address_.c_str(), &addr.sin_addr) != 1) {
        ::close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }
    if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }
    if (::listen(listen_fd_, 16) < 0) {
        ::close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }
    running_ = true;
    stop_ = false;
    worker_ = std::thread([this] { loop(); });
    return true;
#endif
}

void HttpServer::stop() {
#if defined(__linux__)
    stop_ = true;
    if (listen_fd_ >= 0) {
        ::shutdown(listen_fd_, SHUT_RDWR);
        ::close(listen_fd_);
        listen_fd_ = -1;
    }
    if (worker_.joinable()) {
        worker_.join();
    }
    running_ = false;
#endif
}

void HttpServer::loop() {
#if defined(__linux__)
    while (!stop_) {
        sockaddr_in client {};
        socklen_t len = sizeof(client);
        const int fd = ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&client), &len);
        if (fd < 0) {
            if (stop_) {
                break;
            }
            continue;
        }
        std::vector<char> buf(2048);
        const ssize_t n = ::recv(fd, buf.data(), buf.size() - 1, 0);
        std::string req;
        if (n > 0) {
            req.assign(buf.data(), static_cast<std::size_t>(n));
        }
        std::string resp;
        if (req.find("GET /metrics") == 0) {
            const std::string body = metrics_ ? metrics_() : std::string();
            resp = http_response(200, "OK", body, "text/plain; version=0.0.4; charset=utf-8");
        } else if (req.find("GET /health") == 0 || req.find("GET / ") == 0) {
            resp = http_response(200, "OK", "ok\n", "text/plain; charset=utf-8");
        } else {
            resp = http_response(404, "Not Found", "not found\n", "text/plain; charset=utf-8");
        }
        ::send(fd, resp.data(), resp.size(), 0);
        ::close(fd);
    }
#endif
}

}  // namespace lpt
