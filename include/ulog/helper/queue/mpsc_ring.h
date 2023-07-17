#pragma once

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <vector>

#include "power_of_two.h"

namespace ulog {
namespace umq {

template <typename T>
class Producer;

template <typename T>
class Consumer;

template <typename T = char>
class Umq {
  friend class Producer<T>;
  friend class Consumer<T>;
  using size_type = size_t;

 public:
  explicit Umq(size_t num_elements)
      : cons_head_(0), prod_tail_(0), prod_last_(0) {
    if (num_elements < 2) {
      num_elements = 2;
    } else {
      // round up to the next power of 2, since our 'let the indices wrap'
      // technique works only in this case.
      num_elements = queue::RoundUpPowOfTwo(num_elements);
    }
    mask_ = num_elements - 1;
    buffer_.resize(num_elements);
  }

  ~Umq() = default;

 private:
  size_t size() const { return buffer_.size(); }

  inline size_t mask() const { return mask_; }

  size_t inline next_buffer(size_t index) const {
    return (index & ~mask()) + size();
  }

  std::vector<T> buffer_;
  size_t mask_;

  std::atomic<size_t> cons_head_;

  std::atomic<size_t> prod_tail_;
  std::atomic<size_t> prod_last_;
};

template <typename T>
class Producer {
 public:
  explicit Producer(Umq<T> *queue) : ring_(queue), wrapped_(false){}

  ~Producer() = default;

  /**
   * Try to reserve space of size size
   * @param size size of space to reserve
   * @return data pointer if successful, otherwise nullptr
   */
  T *TryReserve(size_t size) {
    const auto out = ring_->cons_head_.load(std::memory_order_relaxed);
    const auto in = ring_->prod_tail_.load(std::memory_order_relaxed);

    const auto new_pos = in + size;
    if (new_pos - out > ring_->size()) {
      return nullptr;
    }

    // Both new position and write_index are in the same range
    if ((new_pos & ring_->mask()) >= size) {
      wrapped_ = false;
      return &ring_->buffer_[in & ring_->mask()];
    }

    // new_pos is in the next block
    if ((out & ring_->mask()) >= size) {
      wrapped_ = true;
      return &ring_->buffer_[0];
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
    const auto in = ring_->prod_tail_.load(std::memory_order_relaxed);

    if (wrapped_) {
      ring_->prod_last_.store(in, std::memory_order_relaxed);
      ring_->prod_tail_.store(ring_->next_buffer(in) + size,
                              std::memory_order_release);
    } else {
      // Whenever we wrap around, we update the last variable to ensure logical
      // consistency.
      if (((in + size) & ring_->mask()) == 0) {
        ring_->prod_last_.store(in + size, std::memory_order_relaxed);
      }
      ring_->prod_tail_.store(in + size, std::memory_order_release);
    }
  }

 private:
  Umq<T> *ring_;
  bool wrapped_;
};

template <typename T>
class Consumer {
 public:
  explicit Consumer(Umq<T> *queue) : ring_(queue) {}

  ~Consumer() = default;

  /**
   * Gets a pointer to the contiguous block in the buffer, and returns the size
   * of that block.
   * @param size returns the size of the contiguous block
   * @return pointer to the contiguous block
   */
  T *TryRead(size_t *size) {
    const auto in = ring_->prod_tail_.load(std::memory_order_acquire);
    const auto last = ring_->prod_last_.load(std::memory_order_relaxed);
    const auto out = ring_->cons_head_.load(std::memory_order_relaxed);

    if (out == in) {
      return nullptr;
    }

    const auto cur_in = in & ring_->mask();
    const auto cur_out = out & ring_->mask();

    // read and write are still in the same block
    if (cur_out < cur_in) {
      *size = in - out;
      return &ring_->buffer_[cur_out];
    }

    // read and write are in different blocks, read the current remaining data
    if (out != last) {
      *size = last - out;
      return &ring_->buffer_[cur_out];
    }

    // The current block has been read, "write" has reached the next block
    const auto cur_block_start = in & ~ring_->mask();
    // Move the read index, which can make room for the write
    ring_->cons_head_.store(cur_block_start, std::memory_order_relaxed);

    if (!cur_in) {
      return nullptr;
    }

    *size = cur_in;
    return &ring_->buffer_[0];
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
    auto out = ring_->cons_head_.load(std::memory_order_relaxed);
    const auto cur_out = out & ring_->mask();
    std::memset(&ring_->buffer_[cur_out], 0, size * sizeof(T));
    ring_->cons_head_.store(out + size, std::memory_order_release);
  }

 private:
  Umq<T> *ring_;
};

}  // namespace umq
}  // namespace ulog
