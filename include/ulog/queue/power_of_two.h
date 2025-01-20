//
// Created by shawnfeng on 23-6-25.
//

#pragma once
#include <cassert>

namespace ulog {
namespace queue {

/**
 * Data structure used when reading data externally
 */
template <typename T = uint8_t>
struct Packet {
  explicit Packet(const size_t s = 0, T *d = nullptr) : size(s), data(d) {}
  Packet(const Packet &other) = default;
  Packet &operator=(const Packet &other) = default;

  Packet(Packet &&other) noexcept : size(other.size), data(other.data) {
    other.data = nullptr;
    other.size = 0;
  }

  explicit operator bool() const noexcept { return data != nullptr; }
  size_t size = 0;
  T *data = nullptr;
};

template <typename T>
static inline constexpr bool is_power_of_2(T x) {
  return ((x) != 0 && (((x) & ((x)-1)) == 0));
}

template <typename T>
static inline auto RoundPowOfTwo(T n) -> decltype(n) {
  uint64_t value = n;

  // Fill 1
  value |= value >> 1U;
  value |= value >> 2U;
  value |= value >> 4U;
  value |= value >> 8U;
  value |= value >> 16U;
  value |= value >> 32U;

  // Unable to round-up, take the value of round-down
  if (decltype(n)(value + 1) == 0) {
    value >>= 1U;
  }

  return value + 1;
}

static inline auto RoundUpPowOfTwo(uint32_t n) -> decltype(n) {
  if (n == 0) return 1;

  // Avoid is already a power of 2
  return RoundPowOfTwo<decltype(n)>(n - 1);
}

static inline auto RoundDownPowOfTwo(uint32_t n) -> decltype(n) { return RoundPowOfTwo<decltype(n)>(n >> 1U); }

/**
 * @brief Check if value is in range [left, right], considering the wraparound
 * case when right < left (overflow)
 */
static inline bool IsInRange(unsigned left, unsigned value, unsigned right) {
  if (right >= left) {
    // Normal
    return (left <= value) && (value <= right);
  } else {
    // Maybe the data overflowed and a wraparound occurred
    return (left <= value) || (value <= right);
  }
}

/**
 * @brief Check if the tail has passed the head, considering the wraparound case when tail < head (overflow)
 */
static bool inline IsPassed(const uint32_t head, const uint32_t tail) { return tail - head < (1U << 31); }

}  // namespace queue
}  // namespace ulog
