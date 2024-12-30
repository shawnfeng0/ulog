//
// Created by shawnfeng on 23-6-25.
//

#pragma once
#include <assert.h>

#include <array>

namespace ulog {
namespace queue {

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

static bool inline IsAllZero(const void* buffer, const size_t size) {
  assert(reinterpret_cast<uintptr_t>(buffer) % 8 == 0);
  assert(size % 8 == 0);
  const auto ptr_uint32_start = static_cast<const uint32_t*>(buffer);
  const auto ptr_uint32_end = ptr_uint32_start + (size / sizeof(*ptr_uint32_start));
  for (auto i = ptr_uint32_start; i < ptr_uint32_end; i++)
    if (*i != 0) return false;
  return true;
}

/**
 * Multithreaded subtraction lock, each thread can increase the size, but only the thread that holds the subtraction
 * lock can perform subtraction until the lock is released.
 */
class AtomSubLock {
  static constexpr uint32_t kSizeMask = (1U << 31) - 1;
  static constexpr uint32_t kLockMask = (1U << 31);

  static uint32_t get_size(const uint32_t num) { return num & kSizeMask; }
  static uint32_t get_locked(const uint32_t num) { return num & kLockMask; }

 public:
  uint32_t size(const std::memory_order m = std::memory_order_acquire) const { return get_size(number_.load(m)); }

  bool lock_for_sub(const std::memory_order m = std::memory_order_acquire) {
    auto old_num = number_.load(std::memory_order_relaxed);

    do {
      // Already locked or size is 0
      if (get_locked(old_num) || get_size(old_num) == 0) return false;
    } while (!number_.compare_exchange_weak(old_num, kLockMask | get_size(old_num), m));

    return true;
  }

  bool unlock_if_not_increased(const uint32_t size, const std::memory_order m = std::memory_order_relaxed) {
    uint32_t expected = size | kLockMask;
    const auto old_num = number_.load(std::memory_order_relaxed);
    assert(get_locked(old_num));

    if (get_size(old_num) != size) return false;
    return number_.compare_exchange_weak(expected, size, m);
  }

  void add(const uint32_t add_size, const std::memory_order m = std::memory_order_release) {
    auto old_num = number_.load(std::memory_order_relaxed);
    uint32_t new_num;
    do {
      new_num = get_locked(old_num) | (get_size(old_num) + add_size);
    } while (!number_.compare_exchange_weak(old_num, new_num, m));
  }

  void sub(const uint32_t sub_size, const std::memory_order m = std::memory_order_relaxed) {
    auto old_num = number_.load(std::memory_order_relaxed);
    assert(get_locked(old_num));

    uint32_t new_num;
    do {
      new_num = kLockMask | (get_size(old_num) - sub_size);
    } while (!number_.compare_exchange_weak(old_num, new_num, m));
  }

 private:
  std::atomic_uint32_t number_{};
};

}  // namespace queue
}  // namespace ulog
