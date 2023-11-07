#pragma once

#include <atomic>
#include <cstddef>
#include <cstdlib>
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

 public:
  explicit Umq(uint32_t num_elements, uint32_t prod_pending_size = 1024) {
    if (num_elements < 2)
      num_elements = 2;
    else {
      // round up to the next power of 2, since our 'let the indices wrap'
      // technique works only in this case.
      num_elements = queue::RoundUpPowOfTwo(num_elements);
    }

    if (prod_pending_size < 2)
      prod_pending_size = 2;
    else {
      prod_pending_size = queue::RoundUpPowOfTwo(prod_pending_size);
    }

    mask_ = num_elements - 1;
    buffer_.resize(num_elements);

    pend_mask_ = prod_pending_size - 1;
    prod_pend_buffer_.resize(prod_pending_size);
  }

  ~Umq() = default;

 private:
  uint32_t size() const { return buffer_.size(); }
  inline uint32_t mask() const { return mask_; }

  uint32_t pend_size() const { return prod_pend_buffer_.size(); }
  inline uint32_t pend_mask() const { return pend_mask_; }

  uint32_t inline next_buffer(uint32_t index) const {
    return (index & ~mask()) + size();
  }

  std::vector<T> buffer_;
  uint32_t mask_;

  std::vector<T> prod_pend_buffer_;
  uint32_t pend_mask_;
  // 32bit prod pending head/tail + 32bit prod head/tail
  std::atomic<uint64_t> prod_head_all_{0};
  std::atomic<uint64_t> prod_tail_all_{0};

  std::atomic<uint32_t> prod_last_{0};
  std::atomic<uint32_t> cons_head_{0};
};

template <typename T>
class Producer {
 public:
  explicit Producer(Umq<T> *queue) : ring_(queue) {}

  ~Producer() = default;

  /**
   * Try to reserve space of size size
   * @param size size of space to reserve
   * @return data pointer if successful, otherwise nullptr
   */
  T *TryReserve(uint32_t size) {
    do {
      const auto cons_head = ring_->cons_head_.load(std::memory_order_relaxed);

      auto prod_head_all =
          ring_->prod_head_all_.load(std::memory_order_relaxed);
      auto prod_pend_head = prod_head_all >> 32;
      auto prod_head = prod_head_all & 0xffffffff;

      const auto prod_tail_all =
          ring_->prod_tail_all_.load(std::memory_order_relaxed);
      const auto prod_pend_tail = prod_tail_all >> 32;

      // Check if there is enough space in the pend queue
      if (prod_pend_head - prod_pend_tail >= ring_->size()) {
        break;
      }

      const auto unused = ring_->size() - (prod_head - cons_head);
      if (unused < size) {
        break;
      }

      // The current block has enough free space
      if (ring_->size() - (prod_head & ring_->mask()) >= size) {
        wrapped_ = false;
        prod_head += size;
        prod_pend_head += 1;
        if (!ring_->prod_head_all_.compare_exchange_strong(
                prod_head_all,
                (prod_pend_head << 32) | (prod_head & 0xffffffff),
                std::memory_order_release)) {
          continue;
        }

        return &ring_->buffer_[prod_head & ring_->mask()];
      }

      // new_pos is in the next block
      if ((cons_head & ring_->mask()) >= size) {
        wrapped_ = true;
        prod_head = ring_->next_buffer(prod_head) + size;
        prod_pend_head += 1;
        if (!ring_->prod_head_all_.compare_exchange_strong(
                prod_head_all,
                (prod_pend_head << 32) | (prod_head & 0xffffffff),
                std::memory_order_release)) {
          continue;
        }
        return &ring_->buffer_[0];
      }
    } while (true);

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
  void Commit(uint32_t size) {
    // only written from push thread
    //    const auto in = ring_->prod_tail_.load(std::memory_order_relaxed);
    //
    //    if (wrapped_) {
    //      ring_->prod_last_.store(in, std::memory_order_relaxed);
    //      ring_->prod_tail_.store(ring_->next_buffer(in) + size,
    //                              std::memory_order_release);
    //    } else {
    //      // Whenever we wrap around, we update the last variable to ensure
    //      logical
    //      // consistency.
    //      if (((in + size) & ring_->mask()) == 0) {
    //        ring_->prod_last_.store(in + size, std::memory_order_relaxed);
    //      }
    //      ring_->prod_tail_.store(in + size, std::memory_order_release);
    //    }
  }

 private:
  Umq<T> *ring_;
  bool wrapped_ = false;
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
  T *TryRead(uint32_t *size) {
    //    const auto in = ring_->prod_tail_.load(std::memory_order_acquire);
    //    const auto last = ring_->prod_last_.load(std::memory_order_relaxed);
    //    const auto out = ring_->cons_head_.load(std::memory_order_relaxed);
    //
    //    if (out == in) {
    //      return nullptr;
    //    }
    //
    //    const auto cur_in = in & ring_->mask();
    //    const auto cur_out = out & ring_->mask();
    //
    //    // read and write are still in the same block
    //    if (cur_out < cur_in) {
    //      *size = in - out;
    //      return &ring_->buffer_[cur_out];
    //    }
    //
    //    // read and write are in different blocks, read the current remaining
    //    data if (out != last) {
    //      *size = last - out;
    //      return &ring_->buffer_[cur_out];
    //    }
    //
    //    // The current block has been read, "write" has reached the next block
    //    const auto cur_block_start = in & ~ring_->mask();
    //    // Move the read index, which can make room for the write
    //    ring_->cons_head_.store(cur_block_start, std::memory_order_relaxed);
    //
    //    if (!cur_in) {
    //      return nullptr;
    //    }
    //
    //    *size = cur_in;
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
  void Release(uint32_t size) {
    auto out = ring_->cons_head_.load(std::memory_order_relaxed);
    ring_->cons_head_.store(out + size, std::memory_order_release);
  }

 private:
  Umq<T> *ring_;
};

}  // namespace umq
}  // namespace ulog
