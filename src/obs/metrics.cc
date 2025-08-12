#include "obs/metrics.h"
#include "net/socket.h"

#include <chrono>
#include <string>
#include <vector>
#include <cstring>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#endif

namespace obs {

void MetricsRegistry::increment(const std::string& name, uint64_t delta) {
  std::lock_guard<std::mutex> lock(mu_);
  counters_[name] += delta;
}

void MetricsRegistry::setGauge(const std::string& name, int64_t value) {
  std::lock_guard<std::mutex> lock(mu_);
  gauges_[name] = value;
}

nlohmann::json MetricsRegistry::snapshot() const {
  std::lock_guard<std::mutex> lock(mu_);
  nlohmann::json j;
  for (const auto& [k, v] : counters_) j["counters"][k] = v;
  for (const auto& [k, v] : gauges_) j["gauges"][k] = v;
  return j;
}

// Very small blocking HTTP server serving one connection at a time
MetricsHttpServer::MetricsHttpServer(MetricsRegistry* registry, const char* host, uint16_t port)
    : registry_(registry), host_(host ? host : "127.0.0.1"), port_(port) {}

MetricsHttpServer::~MetricsHttpServer() { stop(); }

bool MetricsHttpServer::start() {
  if (running_.load()) return false;
  running_.store(true);
  th_ = std::thread(&MetricsHttpServer::run, this);
  return true;
}

void MetricsHttpServer::stop() {
  running_.store(false);
  if (th_.joinable()) th_.join();
}

void MetricsHttpServer::run() {
  // Minimal single-threaded HTTP server (IPv4) serving GET /metrics
#ifdef _WIN32
  WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa);
  SOCKET srv = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (srv == INVALID_SOCKET) { running_.store(false); return; }
  BOOL yes = 1; setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
  sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons(port_);
  inet_pton(AF_INET, host_.c_str(), &addr.sin_addr);
  if (bind(srv, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) { closesocket(srv); running_.store(false); return; }
  listen(srv, 4);
#else
  int srv = ::socket(AF_INET, SOCK_STREAM, 0);
  if (srv < 0) { running_.store(false); return; }
  int yes = 1; setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons(port_);
  inet_pton(AF_INET, host_.c_str(), &addr.sin_addr);
  if (bind(srv, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) { close(srv); running_.store(false); return; }
  listen(srv, 4);
#endif
  while (running_.load()) {
#ifdef _WIN32
    fd_set rfds; FD_ZERO(&rfds); FD_SET(srv, &rfds); TIMEVAL tv{1,0};
    int r = select(0, &rfds, NULL, NULL, &tv);
    if (r <= 0) continue;
    SOCKET cli = accept(srv, NULL, NULL);
    if (cli == INVALID_SOCKET) continue;
    char buf[1024]; int n = recv(cli, buf, sizeof(buf), 0);
#else
    fd_set rfds; FD_ZERO(&rfds); FD_SET(srv, &rfds); timeval tv{1,0};
    int r = select(srv+1, &rfds, NULL, NULL, &tv);
    if (r <= 0) continue;
    int cli = accept(srv, NULL, NULL);
    if (cli < 0) continue;
    char buf[1024]; int n = ::recv(cli, buf, sizeof(buf), 0);
#endif
    if (n <= 0) {
#ifdef _WIN32
      closesocket(cli);
#else
      close(cli);
#endif
      continue;
    }
    // Very simple parse: ensure it's a GET and path starts with /metrics
    std::string req(buf, buf + n);
    bool ok = (req.rfind("GET ", 0) == 0);
    if (ok) {
      size_t sp = req.find(' ');
      size_t sp2 = req.find(' ', sp == std::string::npos ? 0 : sp + 1);
      std::string path = (sp != std::string::npos && sp2 != std::string::npos) ? req.substr(sp + 1, sp2 - sp - 1) : "/";
      if (path != "/metrics" && path != "/") ok = false;
    }
    std::string body;
    int status = 200;
    const char* ctype = "application/json";
    if (ok && registry_) {
      body = registry_->snapshot().dump();
    } else {
      status = 404; ctype = "text/plain"; body = "not found";
    }
    char hdr[256];
    int hl = snprintf(hdr, sizeof(hdr), "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",
                      status, (status==200?"OK":"Not Found"), ctype, body.size());
#ifdef _WIN32
    send(cli, hdr, hl, 0);
    if (!body.empty()) send(cli, body.c_str(), static_cast<int>(body.size()), 0);
    closesocket(cli);
#else
    ::send(cli, hdr, hl, 0);
    if (!body.empty()) ::send(cli, body.c_str(), body.size(), 0);
    close(cli);
#endif
  }
#ifdef _WIN32
  closesocket(srv);
#else
  close(srv);
#endif
}

}  // namespace obs


