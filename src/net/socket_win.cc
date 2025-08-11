#ifdef _WIN32
#include "net/socket.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#define SECURITY_WIN32
#include <schannel.h>
#include <security.h>
#include <wincrypt.h>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Secur32.lib")
#pragma comment(lib, "Crypt32.lib")

namespace net {

namespace {
bool InitWSA() {
  static bool inited = false; static bool ok = false;
  if (!inited) {
    inited = true; WSADATA wsaData; ok = (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);
  }
  return ok;
}
}

class PlainSocketWin : public ISocket {
 public:
  PlainSocketWin() = default;
  ~PlainSocketWin() override { close(); }

  bool connect(const std::string& host, uint16_t port, int timeout_ms) override {
    if (!InitWSA()) return false;
    struct addrinfo hints{}; hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM; hints.ai_protocol = IPPROTO_TCP;
    struct addrinfo* result = nullptr;
    char portStr[16]; _snprintf_s(portStr, sizeof(portStr), _TRUNCATE, "%u", static_cast<unsigned>(port));
    if (getaddrinfo(host.c_str(), portStr, &hints, &result) != 0) return false;
    SOCKET s = INVALID_SOCKET;
    for (auto* p = result; p != nullptr; p = p->ai_next) {
      s = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
      if (s == INVALID_SOCKET) continue;
      // non-blocking connect with timeout
      u_long nonblock = 1; ioctlsocket(s, FIONBIO, &nonblock);
      int c = ::connect(s, p->ai_addr, static_cast<int>(p->ai_addrlen));
      bool ok = false;
      if (c == 0) {
        ok = true;
      } else {
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK || err == WSAEINPROGRESS || err == WSAEINVAL) {
          fd_set wfds; FD_ZERO(&wfds); FD_SET(s, &wfds);
          TIMEVAL tv; tv.tv_sec = timeout_ms/1000; tv.tv_usec = (timeout_ms%1000)*1000;
          int r = select(0, NULL, &wfds, NULL, &tv);
          if (r > 0 && FD_ISSET(s, &wfds)) {
            int so_error = 0; int slen = sizeof(so_error);
            getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&so_error, &slen);
            ok = (so_error == 0);
          }
        }
      }
      nonblock = 0; ioctlsocket(s, FIONBIO, &nonblock);
      if (ok) break;
      closesocket(s); s = INVALID_SOCKET;
    }
    freeaddrinfo(result);
    if (s == INVALID_SOCKET) return false;
    // Set R/W timeouts
    DWORD to = timeout_ms; setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&to), sizeof(to));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&to), sizeof(to));
    sock_ = s; return true;
  }

  int send(const uint8_t* data, size_t len) override {
    if (sock_ == INVALID_SOCKET) return -1;
    int sent = ::send(sock_, reinterpret_cast<const char*>(data), static_cast<int>(len), 0);
    return sent;
  }

  int recv(uint8_t* out, size_t out_cap, int /*timeout_ms*/) override {
    if (sock_ == INVALID_SOCKET) return -1;
    int r = ::recv(sock_, reinterpret_cast<char*>(out), static_cast<int>(out_cap), 0);
    if (r == 0) return 0;
    if (r == SOCKET_ERROR) {
      int err = WSAGetLastError();
      if (err == WSAETIMEDOUT) return -2; // timeout
      return -1;
    }
    return r;
  }

  void close() override {
    if (sock_ != INVALID_SOCKET) { closesocket(sock_); sock_ = INVALID_SOCKET; }
  }

 private:
  SOCKET sock_{INVALID_SOCKET};
};

std::unique_ptr<ISocket> MakePlainSocket() { return std::make_unique<PlainSocketWin>(); }

// For now, TLS is provided via OpenSSL backend (if available). On Windows without OpenSSL,
// return nullptr so callers can detect TLS unsupported.
#ifndef USE_OPENSSL
std::unique_ptr<ISocket> MakeTlsSocket() { return nullptr; }
#endif

}  // namespace net

#endif


