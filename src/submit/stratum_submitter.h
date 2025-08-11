#pragma once

#include <cstdint>
#include <string>

#include "adapters/stratum_runner.h"

namespace submit {

class StratumSubmitter {
 public:
  StratumSubmitter(adapters::StratumRunner* runner, std::string worker_name)
      : runner_(runner), worker_name_(std::move(worker_name)) {}

  // Submit using already-assembled hex fields
  bool submitShareHex(const std::string& extranonce2_hex,
                      const std::string& ntime_hex,
                      const std::string& nonce_hex) {
    if (runner_ == nullptr) return false;
    return runner_->submitShare(extranonce2_hex, ntime_hex, nonce_hex);
  }

  // Extract ntime and nonce from 80-byte big-endian header and submit
  bool submitFromHeader(const uint8_t header80_be[80], const std::string& extranonce2_hex) {
    if (runner_ == nullptr) return false;
    // Offsets for BE header: [0..3]=version, [4..35]=prevhash, [36..67]=merkle,
    // [68..71]=ntime, [72..75]=nbits, [76..79]=nonce
    auto toHex8 = [](const uint8_t* p) {
      static const char* kHex = "0123456789abcdef";
      std::string s; s.resize(8);
      for (int i = 0; i < 4; ++i) {
        s[2*i]   = kHex[(p[i] >> 4) & 0xF];
        s[2*i+1] = kHex[p[i] & 0xF];
      }
      return s;
    };
    const std::string ntime_hex = toHex8(&header80_be[68]);
    const std::string nonce_hex = toHex8(&header80_be[76]);
    return runner_->submitShare(extranonce2_hex, ntime_hex, nonce_hex);
  }

 private:
  adapters::StratumRunner* runner_;
  std::string worker_name_;
};

}  // namespace submit


