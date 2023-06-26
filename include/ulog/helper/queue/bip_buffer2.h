#pragma once

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <vector>

#include "power_of_two.h"

namespace ulog {

template <typename T = char>
class BipBuffer2 {
 public:
  explicit BipBuffer2(size_t num_elements)
      : read_index_(0), write_index_(0), last_index_(0), wrapped_(false) {
    if (num_elements < 2)
      num_elements = 2;
    else {
      // round up to the next power of 2, since our 'let the indices wrap'
      // technique works only in this case.
      num_elements = queue::RoundUpPowOfTwo(num_elements);
    }
    mask_ = num_elements - 1;
    buffer_.resize(num_elements);
  }

  ~BipBuffer2() = default;

  /**
   * Try to reserve space of size size
   * @param size size of space to reserve
   * @return data pointer if successful, otherwise nullptr
   */
  T* Reserve(size_t size) {
    const auto read = read_index_.load(std::memory_order_relaxed);
    const auto write = write_index_.load(std::memory_order_relaxed);

    const auto new_pos = write + size;
    if (new_pos - read > this->size()) {
      return nullptr;
    }

    // Both new position and write_index are in the same range
    if ((new_pos & mask()) >= size) {
      wrapped_ = false;
      return &buffer_[write & mask()];
    }

    // new_pos is in the next block
    if ((read & mask()) >= size) {
      wrapped_ = true;
      return &buffer_[0];
    }

    return nullptr;
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

#if BIPBUFFER2_DEBUG == 1
    printf("Commit: write: %u, size: %u\r\n", write, size);
#endif

    if (wrapped_) {
      last_index_.store(write, std::memory_order_relaxed);
      write_index_.store(next_buffer(write) + size, std::memory_order_release);
    } else {
      // Whenever we wrap around, we update the last variable to ensure logical
      // consistency.
      if (((write + size) & mask()) == 0) {
        last_index_.store(write + size, std::memory_order_relaxed);
      }
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
    const auto read = read_index_.load(std::memory_order_relaxed);

    if (read == write) {
      return nullptr;
    }

    const auto cur_write = write & mask();
    const auto cur_read = read & mask();

    // read and write are still in the same block
    if (cur_read < cur_write) {
      *size = write - read;
      return &buffer_[cur_read];
    }

    // read and write are in different blocks, read the current remaining data
    if (read != last) {
      *size = last - read;
      return &buffer_[cur_read];
    }

    // The current block has been read, "write" has reached the next block
    const auto cur_block_start = write & ~mask();
    // Move the read index, which can make room for the write
    read_index_.store(cur_block_start, std::memory_order_relaxed);

    if (!cur_write) {
      return nullptr;
    }

    *size = cur_write;
    return &buffer_[0];
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
    auto read = read_index_.load(std::memory_order_relaxed);
    read_index_.store(read + size, std::memory_order_release);
  }

 private:
  size_t size() const { return buffer_.size(); }
  inline size_t mask() const { return mask_; }
  size_t inline next_buffer(size_t index) const {
    return (index & ~mask()) + size();
  }

  std::vector<T> buffer_;
  size_t mask_;

  // for reader thread
  std::atomic<size_t> read_index_;

  // for writer thread
  std::atomic<size_t> write_index_;
  std::atomic<size_t> last_index_;
  bool wrapped_;
};
}  // namespace ulog
