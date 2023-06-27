#pragma once

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <vector>

namespace ulog {

template <typename T = char>
class BipBuffer {
 public:
  explicit BipBuffer(size_t buffer_size)
      : buffer_(buffer_size), out_(0), in_(0), last_(0), wrapped_(false) {}

  ~BipBuffer() = default;

  /**
   * Try to reserve space of size size
   * @param size size of space to reserve
   * @return data pointer if successful, otherwise nullptr
   */
  T* TryReserve(size_t size) {
    const auto out = out_.load(std::memory_order_acquire);
    const auto in = in_.load(std::memory_order_relaxed);

    if (in < out) {
      // |********|_____size____||*********|___________|
      // ^0       ^write         ^read     ^last       ^size
      if (in + size < out) {
        wrapped_ = false;
        return &buffer_[in];
      } else {
        return nullptr;
      }

    } else if (in + size <= buffer_.size()) {
      // Check trailing space first
      // |__________|*************|______size______|
      // ^0         ^read         ^write           ^size
      // or
      // |__________|_____________size_____________|
      // ^0         ^read/write                    ^size
      wrapped_ = false;
      return &buffer_[in];

    } else if (out > size) {
      // Check leading space
      // |______size______|*************|__________|
      // ^0               ^read         ^write     ^size
      wrapped_ = true;
      return &buffer_[0];

    } else {
      return nullptr;
    }
  }

  /**
   * Commits the data to the buffer, so that it can be read out.
   * @param size the size of the data to be committed
   *
   * NOTE:
   * The validity of the size is not checked, it needs to be within the range
   * returned by the TryReserve function.
   */
  void Commit(size_t size) {
    // only written from push thread
    const auto in = in_.load(std::memory_order_relaxed);
    const auto last = last_.load(std::memory_order_relaxed);

    if (wrapped_) {
      last_.store(in, std::memory_order_relaxed);
      in_.store(size, std::memory_order_release);
    } else {
      if (last < in + size) last_.store(in + size, std::memory_order_relaxed);
      in_.store(in + size, std::memory_order_release);
    }
  }

  /**
   * Gets a pointer to the contiguous block in the buffer, and returns the size
   * of that block.
   * @param size returns the size of the contiguous block
   * @return pointer to the contiguous block
   */
  T* TryRead(size_t* size) {
    const auto in = in_.load(std::memory_order_acquire);
    const auto last = last_.load(std::memory_order_relaxed);
    auto out = out_.load(std::memory_order_relaxed);

    if (out == in) return nullptr;
    if (out == last && out != 0) {
      out = 0;
      out_.store(out, std::memory_order_release);
    }

    auto out_limit = out <= in ? in : last;
    *size = out_limit - out;

    return (*size == 0) ? nullptr : &buffer_[out];
  }

  /**
   * Releases data from the buffer, so that more data can be written in.
   * @param size the size of the data to be released
   *
   * Note:
   *
   * The validity of the size is not checked, it needs to be within the range
   * returned by the TryRead function.
   *
   */
  void Release(size_t size) { out_.fetch_add(size, std::memory_order_acq_rel); }

 private:
  std::vector<T> buffer_;

  // for producer thread
  std::atomic<size_t> out_;

  // for consumer thread
  std::atomic<size_t> in_;
  std::atomic<size_t> last_;
  bool wrapped_;
};

}  // namespace ulog
