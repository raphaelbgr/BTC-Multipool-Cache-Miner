#pragma once

#include <string>

namespace adapters {

enum class PoolProfile { kGeneric, kViaBTC, kF2Pool };

// Returns formatted username and password for a given pool profile.
// For most pools, username is "<wallet>.<worker>" and password is "x" when unused.
std::pair<std::string, std::string> formatStratumCredentials(
    PoolProfile profile, const std::string& wallet_address, const std::string& worker_name,
    const std::string& password_hint = "x");

}  // namespace adapters



