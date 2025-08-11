#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <memory>

#include <nlohmann/json.hpp>
#include "net/socket.h"

namespace net {

class StratumClient {
 public:
  StratumClient(std::string host, uint16_t port, std::string username, std::string password, bool use_tls = false)
      : host_(std::move(host)), port_(port), username_(std::move(username)), password_(std::move(password)), use_tls_(use_tls) {}

  bool connect();
  void close();

  bool sendSubscribe();
  bool sendAuthorize();

  // Submit share per Stratum V1: mining.submit
  bool sendSubmit(std::string_view worker_name,
                  std::string_view job_id,
                  std::string_view extranonce2_hex,
                  std::string_view ntime_hex,
                  std::string_view nonce_hex);

  enum class RecvStatus { kLine, kTimeout, kClosed };
  // Blocking read with socket timeout. Returns status and, if kLine, the line.
  std::pair<RecvStatus, std::string> recvLine();

  // Utility to send an arbitrary JSON object with a newline.
  bool sendJson(const nlohmann::json& j);

 private:
  std::string host_;
  uint16_t port_{0};
  std::string username_;
  std::string password_;

  bool use_tls_{false};
  std::unique_ptr<class ISocket> sock_;
};

}  // namespace net


