#pragma once

#include <cstdint>
#include <fstream>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <unordered_set>
#include <vector>

namespace store {

struct PendingSubmit {
  uint64_t work_id{0};
  uint32_t nonce{0};
  uint8_t header80[80]{};
};

// Minimal in-memory outbox for submissions, with dedupe by (work_id, nonce).
class Outbox {
 public:
  Outbox() = default;

  void enqueue(const PendingSubmit& s) {
    std::lock_guard<std::mutex> lock(mu_);
    const uint64_t key = (s.work_id << 32) ^ s.nonce;
    if (seen_.count(key)) return;
    seen_.insert(key);
    q_.push(s);
  }

  std::optional<PendingSubmit> tryDequeue() {
    std::lock_guard<std::mutex> lock(mu_);
    if (q_.empty()) return std::nullopt;
    auto s = q_.front();
    q_.pop();
    return s;
  }

  bool empty() const {
    std::lock_guard<std::mutex> lock(mu_);
    return q_.empty();
  }

 private:
  mutable std::mutex mu_;
  std::queue<PendingSubmit> q_;
  std::unordered_set<uint64_t> seen_;

  public:
    // Persistence (binary append log: [work_id(8)][nonce(4)][80 bytes header])
    bool appendToFile(const std::string& filepath, const PendingSubmit& s) {
      std::lock_guard<std::mutex> lock(mu_);
      std::ofstream ofs(filepath, std::ios::binary | std::ios::app);
      if (!ofs) return false;
      ofs.write(reinterpret_cast<const char*>(&s.work_id), sizeof(s.work_id));
      ofs.write(reinterpret_cast<const char*>(&s.nonce), sizeof(s.nonce));
      ofs.write(reinterpret_cast<const char*>(s.header80), sizeof(s.header80));
      return true;
    }

    bool loadFromFile(const std::string& filepath) {
      std::lock_guard<std::mutex> lock(mu_);
      std::ifstream ifs(filepath, std::ios::binary);
      if (!ifs) return false;
      q_ = std::queue<PendingSubmit>();
      seen_.clear();
      while (true) {
        PendingSubmit s{};
        if (!ifs.read(reinterpret_cast<char*>(&s.work_id), sizeof(s.work_id))) break;
        if (!ifs.read(reinterpret_cast<char*>(&s.nonce), sizeof(s.nonce))) break;
        if (!ifs.read(reinterpret_cast<char*>(s.header80), sizeof(s.header80))) break;
        const uint64_t key = (s.work_id << 32) ^ s.nonce;
        if (!seen_.count(key)) {
          seen_.insert(key);
          q_.push(s);
        }
      }
      return true;
    }

    // Remove a specific (work_id, nonce) from the queue and seen set
    bool drop(uint64_t work_id, uint32_t nonce) {
      std::lock_guard<std::mutex> lock(mu_);
      if (q_.empty()) return false;
      bool removed = false;
      std::queue<PendingSubmit> tmp;
      while (!q_.empty()) {
        auto s = q_.front(); q_.pop();
        if (!removed && s.work_id == work_id && s.nonce == nonce) {
          removed = true;
          const uint64_t key = (s.work_id << 32) ^ s.nonce;
          seen_.erase(key);
          continue;
        }
        tmp.push(s);
      }
      q_.swap(tmp);
      return removed;
    }

    // Rewrite file from current queue contents (truncate + append all)
    bool rewriteFile(const std::string& filepath) {
      std::lock_guard<std::mutex> lock(mu_);
      std::ofstream ofs(filepath, std::ios::binary | std::ios::trunc);
      if (!ofs) return false;
      std::queue<PendingSubmit> tmp = q_;
      while (!tmp.empty()) {
        const auto& s = tmp.front();
        ofs.write(reinterpret_cast<const char*>(&s.work_id), sizeof(s.work_id));
        ofs.write(reinterpret_cast<const char*>(&s.nonce), sizeof(s.nonce));
        ofs.write(reinterpret_cast<const char*>(s.header80), sizeof(s.header80));
        tmp.pop();
      }
      return true;
    }

    bool clearFile(const std::string& filepath) {
      std::lock_guard<std::mutex> lock(mu_);
      std::ofstream ofs(filepath, std::ios::binary | std::ios::trunc);
      return static_cast<bool>(ofs);
    }
};

}  // namespace store





