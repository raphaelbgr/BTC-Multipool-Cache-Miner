#include "net/stratum_client.h"
#include "net/socket.h"

#include <cstring>
#include <string>

namespace net {

bool StratumClient::connect() {
  if (use_tls_) {
    sock_ = MakeTlsSocket();
    if (!sock_) return false;
  } else {
    sock_ = MakePlainSocket();
  }
  return sock_->connect(host_, port_, 5000);
}

void StratumClient::close() { if (sock_) { sock_->close(); sock_.reset(); } }

bool StratumClient::sendJson(const nlohmann::json& j) {
  if (!sock_) return false;
  std::string line = j.dump();
  line.append("\r\n");
  const char* data = line.c_str();
  size_t remaining = line.size();
  while (remaining > 0) {
    int sent = sock_->send(reinterpret_cast<const uint8_t*>(data), remaining);
    if (sent <= 0) return false;
    data += sent;
    remaining -= static_cast<size_t>(sent);
  }
  return true;
}

bool StratumClient::sendSubscribe() {
  nlohmann::json j;
  j["id"] = 1;
  j["method"] = "mining.subscribe";
  j["params"] = {"MyCustomBTCMiner/0.1"};
  return sendJson(j);
}

bool StratumClient::sendAuthorize() {
  nlohmann::json j;
  j["id"] = 2;
  j["method"] = "mining.authorize";
  j["params"] = {username_, password_};
  return sendJson(j);
}

bool StratumClient::sendSubmit(std::string_view worker_name,
                               std::string_view job_id,
                               std::string_view extranonce2_hex,
                               std::string_view ntime_hex,
                               std::string_view nonce_hex) {
  nlohmann::json j;
  j["id"] = 3;
  j["method"] = "mining.submit";
  j["params"] = {worker_name, job_id, extranonce2_hex, ntime_hex, nonce_hex};
  return sendJson(j);
}

std::pair<StratumClient::RecvStatus, std::string> StratumClient::recvLine() {
  if (!sock_) return {RecvStatus::kClosed, {}};
  std::string line;
  char ch;
  while (true) {
    int recvd = sock_->recv(reinterpret_cast<uint8_t*>(&ch), 1, 5000);
    if (recvd <= 0) {
      if (recvd == -2) return {RecvStatus::kTimeout, {}};
      if (line.empty()) return {RecvStatus::kClosed, {}};
      break;
    }
    if (ch == '\n') break;
    line.push_back(ch);
  }
  return {RecvStatus::kLine, line};
}

}  // namespace net


