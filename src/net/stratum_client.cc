#include "net/stratum_client.h"

#include <cstring>
#include <string>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "Ws2_32.lib")
#else
#  include <arpa/inet.h>
#  include <netdb.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>
#endif

namespace net {

static bool initSockets() {
#ifdef _WIN32
  WSADATA wsaData;
  return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
#else
  return true;
#endif
}

static void cleanupSockets() {
#ifdef _WIN32
  WSACleanup();
#endif
}

bool StratumClient::connect() {
  if (!initSockets()) return false;

  struct addrinfo hints;
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  struct addrinfo* result = nullptr;
  char portStr[16];
  std::snprintf(portStr, sizeof(portStr), "%u", static_cast<unsigned>(port_));
  if (getaddrinfo(host_.c_str(), portStr, &hints, &result) != 0) {
    cleanupSockets();
    return false;
  }

  int sockfd = -1;
  for (auto* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
    sockfd = static_cast<int>(::socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol));
    if (sockfd < 0) continue;
    if (::connect(sockfd, ptr->ai_addr, static_cast<int>(ptr->ai_addrlen)) == 0) {
      break; // success
    }
#ifdef _WIN32
    closesocket(sockfd);
#else
    close(sockfd);
#endif
    sockfd = -1;
  }
  freeaddrinfo(result);
  if (sockfd < 0) {
    cleanupSockets();
    return false;
  }
  sock_ = sockfd;
  return true;
}

void StratumClient::close() {
  if (sock_ >= 0) {
#ifdef _WIN32
    closesocket(sock_);
#else
    ::close(sock_);
#endif
    sock_ = -1;
  }
  cleanupSockets();
}

bool StratumClient::sendJson(const nlohmann::json& j) {
  if (sock_ < 0) return false;
  std::string line = j.dump();
  line.push_back('\n');
  const char* data = line.c_str();
  size_t remaining = line.size();
  while (remaining > 0) {
#ifdef _WIN32
    int sent = ::send(sock_, data, static_cast<int>(remaining), 0);
#else
    ssize_t sent = ::send(sock_, data, remaining, 0);
#endif
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

std::optional<std::string> StratumClient::recvLine() {
  if (sock_ < 0) return std::nullopt;
  std::string line;
  char ch;
  while (true) {
#ifdef _WIN32
    int recvd = ::recv(sock_, &ch, 1, 0);
#else
    ssize_t recvd = ::recv(sock_, &ch, 1, 0);
#endif
    if (recvd <= 0) {
      if (line.empty()) return std::nullopt;
      break;
    }
    if (ch == '\n') break;
    line.push_back(ch);
  }
  return line;
}

}  // namespace net


