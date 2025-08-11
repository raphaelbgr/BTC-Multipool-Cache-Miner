#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

namespace cuda_engine {

struct HitRecord {
  uint64_t work_id{0};
  uint32_t nonce{0};
};

class HitRingBuffer {
 public:
  explicit HitRingBuffer(std::size_t capacity)
      : capacity_(capacity), buffer_(capacity) {}

  std::size_t capacity() const { return capacity_; }

  // Push a hit; overwrites oldest when full. Returns true if overwrote.
  bool push(const HitRecord& hit) {
    std::lock_guard<std::mutex> lock(mu_);
    bool overwrote = size_ == capacity_;
    buffer_[tail_] = hit;
    tail_ = (tail_ + 1) % capacity_;
    if (overwrote) {
      head_ = (head_ + 1) % capacity_;
    } else {
      ++size_;
    }
    return overwrote;
  }

  // Pop oldest hit if available.
  std::optional<HitRecord> tryPop() {
    std::lock_guard<std::mutex> lock(mu_);
    if (size_ == 0) return std::nullopt;
    HitRecord out = buffer_[head_];
    head_ = (head_ + 1) % capacity_;
    --size_;
    return out;
  }

  std::size_t size() const {
    std::lock_guard<std::mutex> lock(mu_);
    return size_;
  }

  bool empty() const { return size() == 0; }

 private:
  const std::size_t capacity_;
  mutable std::mutex mu_;
  std::vector<HitRecord> buffer_;
  std::size_t head_{0};
  std::size_t tail_{0};
  std::size_t size_{0};
};

}  // namespace cuda_engine



