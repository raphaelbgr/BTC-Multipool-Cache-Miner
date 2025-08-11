// Cross-platform socket abstraction with optional TLS backends.
#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>

namespace net {

// Return codes for recv: >=0 = bytes read, 0 = closed, -1 = error, -2 = timeout
class ISocket {
 public:
  virtual ~ISocket() = default;
  virtual bool connect(const std::string& host, uint16_t port, int timeout_ms) = 0;
  virtual int send(const uint8_t* data, size_t len) = 0;
  virtual int recv(uint8_t* out, size_t out_cap, int timeout_ms) = 0;
  virtual void close() = 0;
};

// Factory helpers (implemented per platform)
std::unique_ptr<ISocket> MakePlainSocket();
std::unique_ptr<ISocket> MakeTlsSocket();

}  // namespace net


