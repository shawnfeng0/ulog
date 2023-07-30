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
      : out_(0), in_(0), last_(0), wrapped_(false) {
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
  T *TryReserve(size_t size) {
    const auto out = out_.load(std::memory_order_relaxed);
    const auto in = in_.load(std::memory_order_relaxed);

    const auto unused = this->size() - (in - out);
    if (unused < size) {
      return nullptr;
    }

    // The current block has enough free space
    if (this->size() - (in & mask()) >= size) {
      wrapped_ = false;
      return &buffer_[in & mask()];
    }

    // new_pos is in the next block
    if ((out & mask()) >= size) {
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
   * returned by the TryReserve function.
   */
  void Commit(size_t size) {
    // only written from push thread
    const auto in = in_.load(std::memory_order_relaxed);

    if (wrapped_) {
      last_.store(in, std::memory_order_relaxed);
      in_.store(next_buffer(in) + size, std::memory_order_release);
    } else {
      // Whenever we wrap around, we update the last variable to ensure logical
      // consistency.
      const auto new_pos = in + size;
      if ((new_pos & mask()) == 0) {
        last_.store(new_pos, std::memory_order_relaxed);
      }
      in_.store(new_pos, std::memory_order_release);
    }
  }

  /**
   * Gets a pointer to the contiguous block in the buffer, and returns the size
   * of that block.
   * @param size returns the size of the contiguous block
   * @return pointer to the contiguous block
   */
  T *TryRead(size_t *size) {
    const auto in = in_.load(std::memory_order_acquire);
    const auto last = last_.load(std::memory_order_relaxed);
    const auto out = out_.load(std::memory_order_relaxed);

    if (out == in) {
      return nullptr;
    }

    const auto cur_in = in & mask();
    const auto cur_out = out & mask();

    // read and write are still in the same block
    if (cur_out < cur_in) {
      *size = in - out;
      return &buffer_[cur_out];
    }

    // read and write are in different blocks, read the current remaining data
    if (out != last) {
      *size = last - out;
      return &buffer_[cur_out];
    }

    // The current block has been read, "write" has reached the next block
    const auto cur_block_start = in & ~mask();
    // Move the read index, which can make room for the writer
    out_.store(cur_block_start, std::memory_order_relaxed);

    if (!cur_in) {
      return nullptr;
    }

    *size = cur_in;
    return &buffer_[0];
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
  void Release(size_t size) {
    auto out = out_.load(std::memory_order_relaxed);
    out_.store(out + size, std::memory_order_release);
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
  std::atomic<size_t> out_;

  // for writer thread
  std::atomic<size_t> in_;
  std::atomic<size_t> last_;
  bool wrapped_;
};
}  // namespace ulog
