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
      : buffer_(buffer_size),
        read_index_(0),
        write_index_(0),
        last_index_(0),
        write_wrapped_(false) {}

  ~BipBuffer() = default;

  /**
   * Try to reserve space of size size
   * @param size size of space to reserve
   * @return data pointer if successful, otherwise nullptr
   */
  T* Reserve(size_t size) {
    const auto read = read_index_.load(std::memory_order_acquire);
    const auto write = write_index_.load(std::memory_order_relaxed);

    if (write < read) {
      // |********|_____size____||*********|___________|
      // ^0       ^write         ^read     ^last       ^size
      if (write + size < read) {
        write_wrapped_ = false;
        return &buffer_[write];
      } else {
        return nullptr;
      }

    } else if (write + size <= buffer_.size()) {
      // Check trailing space first
      // |__________|*************|______size______|
      // ^0         ^read         ^write           ^size
      // or
      // |__________|_____________size_____________|
      // ^0         ^read/write                    ^size
      write_wrapped_ = false;
      return &buffer_[write];

    } else if (read > size) {
      // Check leading space
      // |______size______|*************|__________|
      // ^0               ^read         ^write     ^size
      write_wrapped_ = true;
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
   * returned by the Reserve function.
   */
  void Commit(size_t size) {
    // only written from push thread
    const auto write = write_index_.load(std::memory_order_relaxed);
    const auto last = last_index_.load(std::memory_order_relaxed);

    if (write_wrapped_) {
      last_index_.store(write, std::memory_order_relaxed);
      write_index_.store(size, std::memory_order_release);
    } else {
      if (last < write + size)
        last_index_.store(write + size, std::memory_order_relaxed);
      write_index_.store(write + size, std::memory_order_release);
    }
  }

  /**
   * Gets a pointer to the contiguous block in the buffer, and returns the size
   * of that block.
   * @param size returns the size of the contiguous block
   * @return pointer to the contiguous block
   */
  T* Read(size_t* size) {
    const auto write = write_index_.load(std::memory_order_acquire);
    const auto last = last_index_.load(std::memory_order_relaxed);
    auto read = read_index_.load(std::memory_order_relaxed);

    if (read == write) return nullptr;
    if (read == last && read != 0) {
      read = 0;
      read_index_.store(read, std::memory_order_release);
    }

    auto read_limit = read <= write ? write : last;
    *size = read_limit - read;

    return (*size == 0) ? nullptr : &buffer_[read];
  }

  /**
   * Releases data from the buffer, so that more data can be written in.
   * @param size the size of the data to be released
   *
   * Note:
   *
   * The validity of the size is not checked, it needs to be within the range
   * returned by the Read function.
   *
   */
  void Release(size_t size) {
    read_index_.fetch_add(size, std::memory_order_acq_rel);
  }

 private:
  std::vector<T> buffer_;

  // for reader thread
  std::atomic<size_t> read_index_;

  // for writer thread
  std::atomic<size_t> write_index_;
  std::atomic<size_t> last_index_;
  bool write_wrapped_;
};

}  // namespace ulog
