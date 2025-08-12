#ifndef _WIN32
#include "net/socket.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>

namespace net {

namespace {
int set_nonblock(int fd, bool nb) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0) return -1;
  return fcntl(fd, F_SETFL, nb ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK));
}
}

class PlainSocketPosix : public ISocket {
 public:
  PlainSocketPosix() = default;
  ~PlainSocketPosix() override { close(); }

  bool connect(const std::string& host, uint16_t port, int timeout_ms) override {
    struct addrinfo hints{}; hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM; hints.ai_protocol = IPPROTO_TCP;
    struct addrinfo* res = nullptr; char pstr[16]; std::snprintf(pstr, sizeof(pstr), "%u", (unsigned)port);
    if (getaddrinfo(host.c_str(), pstr, &hints, &res) != 0) return false;
    int fd = -1;
    for (auto* p = res; p; p = p->ai_next) {
      fd = static_cast<int>(::socket(p->ai_family, p->ai_socktype, p->ai_protocol));
      if (fd < 0) continue;
      set_nonblock(fd, true);
      int c = ::connect(fd, p->ai_addr, (socklen_t)p->ai_addrlen);
      bool ok = false;
      if (c == 0) {
        ok = true;
      } else if (errno == EINPROGRESS) {
        fd_set wfds; FD_ZERO(&wfds); FD_SET(fd, &wfds);
        struct timeval tv; tv.tv_sec = timeout_ms/1000; tv.tv_usec = (timeout_ms%1000)*1000;
        int r = select(fd + 1, nullptr, &wfds, nullptr, &tv);
        if (r > 0 && FD_ISSET(fd, &wfds)) {
          int so_error = 0; socklen_t slen = sizeof(so_error);
          getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_error, &slen);
          ok = (so_error == 0);
        }
      }
      set_nonblock(fd, false);
      if (ok) { sock_ = fd; break; }
      ::close(fd); fd = -1;
    }
    freeaddrinfo(res);
    return sock_ >= 0;
  }

  int send(const uint8_t* data, size_t len) override {
    if (sock_ < 0) return -1;
    ssize_t r = ::send(sock_, data, len, 0);
    return r < 0 ? -1 : (int)r;
  }

  int recv(uint8_t* out, size_t out_cap, int timeout_ms) override {
    if (sock_ < 0) return -1;
    fd_set rfds; FD_ZERO(&rfds); FD_SET(sock_, &rfds);
    struct timeval tv; tv.tv_sec = timeout_ms/1000; tv.tv_usec = (timeout_ms%1000)*1000;
    int rsel = select(sock_ + 1, &rfds, nullptr, nullptr, &tv);
    if (rsel == 0) return -2; // timeout
    if (rsel < 0) return -1;
    ssize_t r = ::recv(sock_, out, out_cap, 0);
    if (r == 0) return 0;
    return r < 0 ? -1 : (int)r;
  }

  void close() override {
    if (sock_ >= 0) { ::close(sock_); sock_ = -1; }
  }

 private:
  int sock_{-1};
};

std::unique_ptr<ISocket> MakePlainSocket() { return std::make_unique<PlainSocketPosix>(); }

#ifndef USE_OPENSSL
std::unique_ptr<ISocket> MakeTlsSocket() { return nullptr; }
#endif

}  // namespace net

#endif // _WIN32


