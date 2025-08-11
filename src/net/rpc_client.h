#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace net {

// Minimal JSON-RPC 1.0 client for Bitcoin Core over HTTP.
class RpcClient {
 public:
  explicit RpcClient(std::string url)
      : url_(std::move(url)) {}

  // If username is empty and auth is cookie, this will attempt to read the cookie file automatically.
  void setBasicAuth(std::string username, std::string password) {
    username_ = std::move(username); password_ = std::move(password);
  }

  void setCookieAuthFile(std::string cookie_path) { cookie_path_ = std::move(cookie_path); }

  // Performs a JSON-RPC call. Returns result object or nullopt on error. If error, last_error_ set.
  std::optional<nlohmann::json> call(std::string_view method, const nlohmann::json& params);

  std::string lastError() const { return last_error_; }

 private:
  bool ensureCookie();
  bool httpPostJson(const std::string& body, std::string& out_response);

  std::string url_;
  std::string username_;
  std::string password_;
  std::string cookie_path_;
  std::string last_error_;
};

}  // namespace net


