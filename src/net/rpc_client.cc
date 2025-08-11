#include "net/rpc_client.h"

#include <fstream>
#include <regex>
#include <sstream>

#include "net/socket.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace net {

static std::string detectDefaultCookiePath() {
#ifdef _WIN32
  char* appdata = nullptr;
  size_t len = 0;
  _dupenv_s(&appdata, &len, "APPDATA");
  if (appdata && len > 0) {
    std::string path = std::string(appdata) + "\\Bitcoin\\.cookie";
    free(appdata);
    return path;
  }
  return "C:\\Users\\Public\\AppData\\Roaming\\Bitcoin\\.cookie";
#else
  const char* home = getenv("HOME");
  if (!home) return {};
  return std::string(home) + "/.bitcoin/.cookie";
#endif
}

bool RpcClient::ensureCookie() {
  if (!username_.empty()) return true; // basic already set
  std::string path = cookie_path_.empty() ? detectDefaultCookiePath() : cookie_path_;
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs.good()) { last_error_ = "cookie_not_found"; return false; }
  std::string content; std::getline(ifs, content);
  // cookie file is username:password
  auto pos = content.find(":");
  if (pos == std::string::npos) { last_error_ = "invalid_cookie"; return false; }
  username_ = content.substr(0, pos);
  password_ = content.substr(pos + 1);
  return true;
}

// Very small built-in HTTP client using plain sockets; assumes URL like http://host:port
// For simplicity we limit to http and rely on bitcoind being local.
bool RpcClient::httpPostJson(const std::string& body, std::string& out_response) {
  // Parse URL
  // Format: http://host:port or http://host
  std::regex re(R"((http|https)://([^/:]+)(?::(\d+))?(.*))");
  std::smatch m;
  if (!std::regex_match(url_, m, re)) { last_error_ = "bad_url"; return false; }
  const std::string scheme = m[1];
  const std::string host = m[2];
  const uint16_t port = m[3].matched ? static_cast<uint16_t>(std::stoi(m[3])) : (scheme == "https" ? 443 : 80);
  const std::string path = m[4].matched && !std::string(m[4]).empty() ? std::string(m[4]) : std::string("/");

  std::unique_ptr<class ISocket> sock = (scheme == "https") ? MakeTlsSocket() : MakePlainSocket();
  if (!sock || !sock->connect(host, port, 3000)) { last_error_ = "connect_failed"; return false; }

  // Basic auth header
  std::string auth_header;
  if (!username_.empty()) {
    std::string up = username_ + ":" + password_;
    // naive base64
    static const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string enc; enc.reserve((up.size() * 4) / 3 + 4);
    size_t i = 0; unsigned int val = 0; int valb = -6;
    for (unsigned char c : up) {
      val = (val << 8) + c; valb += 8;
      while (valb >= 0) { enc.push_back(b64[(val >> valb) & 0x3F]); valb -= 6; }
    }
    if (valb > -6) enc.push_back(b64[((val << 8) >> (valb + 8)) & 0x3F]);
    while (enc.size() % 4) enc.push_back('=');
    auth_header = "Authorization: Basic " + enc + "\r\n";
  }

  std::ostringstream req;
  req << "POST " << path << " HTTP/1.1\r\n";
  req << "Host: " << host << "\r\n";
  req << "User-Agent: MyCustomBTCMiner/0.1\r\n";
  req << "Content-Type: application/json\r\n";
  req << auth_header;
  req << "Content-Length: " << body.size() << "\r\n";
  req << "Connection: close\r\n\r\n";
  req << body;
  const std::string req_str = req.str();
  size_t remaining = req_str.size();
  const uint8_t* data = reinterpret_cast<const uint8_t*>(req_str.data());
  while (remaining > 0) {
    int sent = sock->send(data, remaining);
    if (sent <= 0) { last_error_ = "send_failed"; return false; }
    data += sent; remaining -= static_cast<size_t>(sent);
  }
  // Read response
  std::string resp;
  uint8_t buf[4096];
  while (true) {
    int r = sock->recv(buf, sizeof(buf), 5000);
    if (r == 0) break; if (r < 0) { if (r == -2) continue; last_error_ = "recv_failed"; return false; }
    resp.append(reinterpret_cast<const char*>(buf), r);
  }
  // Very simple HTTP parse: split headers/body on CRLF CRLF
  auto pos = resp.find("\r\n\r\n");
  if (pos == std::string::npos) { last_error_ = "bad_http"; return false; }
  out_response = resp.substr(pos + 4);
  return true;
}

std::optional<nlohmann::json> RpcClient::call(std::string_view method, const nlohmann::json& params) {
  if (!ensureCookie() && username_.empty()) {
    return std::nullopt;
  }
  nlohmann::json j;
  j["jsonrpc"] = "1.0";
  j["id"] = 1;
  j["method"] = method;
  j["params"] = params;
  std::string body = j.dump();
  std::string resp_body;
  if (!httpPostJson(body, resp_body)) return std::nullopt;
  auto resp = nlohmann::json::parse(resp_body, nullptr, false);
  if (resp.is_discarded()) { last_error_ = "bad_json"; return std::nullopt; }
  if (!resp.contains("error")) return std::nullopt;
  if (!resp["error"].is_null()) { last_error_ = resp["error"].dump(); return std::nullopt; }
  if (!resp.contains("result")) { last_error_ = "no_result"; return std::nullopt; }
  return resp["result"];
}

}  // namespace net


