#include "adapters/pool_profiles.h"

namespace adapters {

std::pair<std::string, std::string> formatStratumCredentials(
    PoolProfile profile, const std::string& wallet_address, const std::string& worker_name,
    const std::string& password_hint) {
  std::string user = wallet_address;
  if (!worker_name.empty()) {
    user.push_back('.');
    user.append(worker_name);
  }
  std::string pass = password_hint.empty() ? std::string("x") : password_hint;
  switch (profile) {
    case PoolProfile::kViaBTC:
      // ViaBTC typically accepts "wallet.worker" / pass "x"
      break;
    case PoolProfile::kF2Pool:
      // F2Pool format is also "wallet.worker"; some accounts require account-based names.
      break;
    case PoolProfile::kGeneric:
    default:
      break;
  }
  return {user, pass};
}

}  // namespace adapters



