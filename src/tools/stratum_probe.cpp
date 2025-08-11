#include <iostream>
#include <string>

#include "net/stratum_client.h"
#include "adapters/pool_profiles.h"

int main(int argc, char** argv) {
  if (argc < 3) {
    std::cerr << "Usage: stratum_probe <host:port> <btc_address>[.worker] [password]\n";
    return 1;
  }
  std::string hostport = argv[1];
  // argv[2]: wallet; argv[3]: optional worker; argv[4]: optional pass; argv[5]: optional profile
  std::string wallet = argv[2];
  std::string worker = (argc >= 4) ? argv[3] : "bench";
  std::string pass = (argc >= 5) ? argv[4] : "x";
  adapters::PoolProfile profile = adapters::PoolProfile::kGeneric;
  if (argc >= 6) {
    std::string prof = argv[5];
    if (prof == "viabtc") profile = adapters::PoolProfile::kViaBTC;
    else if (prof == "f2pool") profile = adapters::PoolProfile::kF2Pool;
  }
  auto creds = adapters::formatStratumCredentials(profile, wallet, worker, pass);
  std::string user = creds.first;
  pass = creds.second;

  auto pos = hostport.find(":");
  if (pos == std::string::npos) {
    std::cerr << "Invalid host:port\n";
    return 1;
  }
  std::string host = hostport.substr(0, pos);
  uint16_t port = static_cast<uint16_t>(std::stoi(hostport.substr(pos + 1)));

  net::StratumClient c(host, port, user, pass);
  if (!c.connect()) {
    std::cerr << "Connect failed\n";
    return 2;
  }
  std::cout << "Connected to " << host << ":" << port << "\n";
  if (!c.sendSubscribe()) {
    std::cerr << "Subscribe send failed\n";
    return 3;
  }
  if (!c.sendAuthorize()) {
    std::cerr << "Authorize send failed\n";
    return 4;
  }
  // Read a few lines
  for (int i = 0; i < 5; ++i) {
    auto [status, line] = c.recvLine();
    if (status != net::StratumClient::RecvStatus::kLine) break;
    std::cout << line << "\n";
  }
  c.close();
  return 0;
}


