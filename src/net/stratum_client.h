#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace net {

class StratumClient {
 public:
  StratumClient(std::string host, uint16_t port, std::string username, std::string password)
      : host_(std::move(host)), port_(port), username_(std::move(username)), password_(std::move(password)) {}

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

  // Returns one JSON message string if available from socket, else empty.
  std::optional<std::string> recvLine();

  // Utility to send an arbitrary JSON object with a newline.
  bool sendJson(const nlohmann::json& j);

 private:
  std::string host_;
  uint16_t port_{0};
  std::string username_;
  std::string password_;

  int sock_{-1};
};

}  // namespace net


