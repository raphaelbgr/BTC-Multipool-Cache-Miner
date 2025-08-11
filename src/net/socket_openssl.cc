#ifdef USE_OPENSSL
#include "net/socket.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <string>
#include <cstring>

namespace net {

namespace {
#ifdef _WIN32
bool InitWSA() {
  static bool inited=false, ok=false; if (inited) return ok; inited=true; WSADATA w; ok = (WSAStartup(MAKEWORD(2,2), &w) == 0); return ok;
}
#endif

int set_nonblock(int fd, bool nb) {
#ifdef _WIN32
  u_long mode = nb ? 1 : 0; return ioctlsocket((SOCKET)fd, FIONBIO, &mode);
#else
  int flags = fcntl(fd, F_GETFL, 0); if (flags < 0) return -1; return fcntl(fd, F_SETFL, nb ? (flags|O_NONBLOCK) : (flags & ~O_NONBLOCK));
#endif
}

int fd_send(int fd, const uint8_t* data, size_t len) {
#ifdef _WIN32
  return ::send((SOCKET)fd, reinterpret_cast<const char*>(data), (int)len, 0);
#else
  return (int)::send(fd, data, len, 0);
#endif
}

int fd_recv(int fd, uint8_t* out, size_t cap) {
#ifdef _WIN32
  return ::recv((SOCKET)fd, reinterpret_cast<char*>(out), (int)cap, 0);
#else
  return (int)::recv(fd, out, cap, 0);
#endif
}

int fd_close(int fd){
#ifdef _WIN32
  return closesocket((SOCKET)fd);
#else
  return ::close(fd);
#endif
}

bool wait_fd(int fd, bool write, int timeout_ms) {
#ifdef _WIN32
  fd_set fds; FD_ZERO(&fds); FD_SET((SOCKET)fd, &fds);
  TIMEVAL tv; tv.tv_sec = timeout_ms/1000; tv.tv_usec = (timeout_ms%1000)*1000;
  int r = select(0, write?NULL:&fds, write?&fds:NULL, NULL, &tv);
  return r>0;
#else
  fd_set rset, wset; FD_ZERO(&rset); FD_ZERO(&wset);
  if (write) FD_SET(fd, &wset); else FD_SET(fd, &rset);
  struct timeval tv; tv.tv_sec = timeout_ms/1000; tv.tv_usec = (timeout_ms%1000)*1000;
  int r = select(fd+1, write?NULL:&rset, write?&wset:NULL, NULL, &tv);
  return r>0;
#endif
}
}

class TlsSocketOpenSSL : public ISocket {
 public:
  TlsSocketOpenSSL() = default;
  ~TlsSocketOpenSSL() override { close(); }

  bool connect(const std::string& host, uint16_t port, int timeout_ms) override {
#ifdef _WIN32
    if (!InitWSA()) return false;
#endif
    // Resolve and connect TCP
    struct addrinfo hints{}; hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM; hints.ai_protocol = IPPROTO_TCP;
    struct addrinfo* res=nullptr; char pstr[16]; std::snprintf(pstr, sizeof(pstr), "%u", (unsigned)port);
    if (getaddrinfo(host.c_str(), pstr, &hints, &res) != 0) return false;
    int fd = -1;
    for (auto* p=res; p; p=p->ai_next) {
#ifdef _WIN32
      SOCKET s = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
      if (s == INVALID_SOCKET) continue; fd = (int)s;
#else
      fd = (int)::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
      if (fd < 0) continue;
#endif
      set_nonblock(fd, true);
      int c = ::connect(
#ifdef _WIN32
        (SOCKET)
#endif
        fd, p->ai_addr, (int)p->ai_addrlen);
      if (c == 0 || wait_fd(fd, true, timeout_ms)) { set_nonblock(fd, false); break; }
      fd_close(fd); fd = -1;
    }
    freeaddrinfo(res);
    if (fd < 0) return false;

    // OpenSSL init
    SSL_load_error_strings(); OpenSSL_add_ssl_algorithms();
    ctx_ = SSL_CTX_new(TLS_client_method()); if (!ctx_) { fd_close(fd); return false; }
    ssl_ = SSL_new(ctx_); if (!ssl_) { SSL_CTX_free(ctx_); ctx_=nullptr; fd_close(fd); return false; }
    SSL_set_tlsext_host_name(ssl_, host.c_str());
    SSL_set_fd(ssl_, fd);

    // Non-blocking handshake with timeout
    set_nonblock(fd, true);
    int hs_to = timeout_ms;
    while (true) {
      int r = SSL_connect(ssl_);
      if (r == 1) break;
      int e = SSL_get_error(ssl_, r);
      if (e == SSL_ERROR_WANT_READ) { if (!wait_fd(fd, false, hs_to)) { cleanup_fd(fd); return false; } }
      else if (e == SSL_ERROR_WANT_WRITE) { if (!wait_fd(fd, true, hs_to)) { cleanup_fd(fd); return false; } }
      else { cleanup_fd(fd); return false; }
    }
    set_nonblock(fd, false);
    fd_ = fd; return true;
  }

  int send(const uint8_t* data, size_t len) override {
    if (!ssl_) return -1; int r = SSL_write(ssl_, data, (int)len); return r <= 0 ? -1 : r;
  }

  int recv(uint8_t* out, size_t cap, int timeout_ms) override {
    if (!ssl_) return -1;
    int r = SSL_read(ssl_, out, (int)cap);
    if (r > 0) return r;
    int e = SSL_get_error(ssl_, r);
    if (e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE) {
      // Signal to caller that no data is available yet (non-fatal)
      return -2;
    }
    return (r == 0) ? 0 : -1;
  }

  void close() override {
    if (ssl_) { SSL_shutdown(ssl_); SSL_free(ssl_); ssl_ = nullptr; }
    if (ctx_) { SSL_CTX_free(ctx_); ctx_ = nullptr; }
    if (fd_ >= 0) { fd_close(fd_); fd_ = -1; }
    EVP_cleanup();
  }

 private:
  void cleanup_fd(int fd){ if (ssl_) { SSL_free(ssl_); ssl_=nullptr; } if (ctx_) { SSL_CTX_free(ctx_); ctx_=nullptr; } fd_close(fd); }

  SSL_CTX* ctx_{nullptr};
  SSL* ssl_{nullptr};
  int fd_{-1};
};

std::unique_ptr<ISocket> MakeTlsSocket() { return std::make_unique<TlsSocketOpenSSL>(); }

} // namespace net

#endif // USE_OPENSSL


