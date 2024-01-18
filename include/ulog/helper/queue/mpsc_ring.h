#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>

#include "power_of_two.h"

namespace ulog {
namespace umq {

class Producer;
class Consumer;

struct Packet {
  uint32_t size : 30;
  uint32_t submitted : 1;
  uint32_t discarded : 1;
  uint8_t data[0];
};

union PacketPtr {
  explicit PacketPtr(void *ptr) : ptr_(ptr) {}
  auto operator->() const -> Packet * { return header; }
  auto operator=(void *ptr) -> PacketPtr & {
    this->ptr_ = ptr;
    return *this;
  }

 private:
  Packet *header;
  __attribute__((unused)) void *ptr_;
};

class Umq {
  friend class Producer;
  friend class Consumer;

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
    data_ = std::make_unique<uint8_t[]>(mask_ + 1);
  }

  ~Umq() = default;

 private:
  size_t size() const { return mask_ + 1; }

  inline size_t mask() const { return mask_; }

  size_t inline next_buffer(size_t index) const {
    return (index & ~mask()) + size();
  }

  std::unique_ptr<uint8_t[]> data_;  // the buffer holding the data
  size_t mask_;

  std::atomic<size_t> cons_head_;

  std::atomic<size_t> prod_tail_;
  std::atomic<size_t> prod_last_;
};

class Producer {
 public:
  explicit Producer(Umq *queue) : ring_(queue) {}

  ~Producer() = default;

  /**
   * Try to reserve space of size size
   * @param size size of space to reserve
   * @return data pointer if successful, otherwise nullptr
   */
  void *TryReserve(size_t size) {
    const auto out = ring_->cons_head_.load(std::memory_order_relaxed);
    const auto in = ring_->prod_tail_.load(std::memory_order_relaxed);

    auto real_size = size + sizeof(Packet);

    const auto new_pos = in + real_size;
    if (new_pos - out > ring_->size()) {
      return nullptr;
    }

    uint8_t *data_ptr = nullptr;
    // Both new position and write_index are in the same range
    if ((new_pos & ring_->mask()) >= real_size) {
      data_ptr = &ring_->data_[in & ring_->mask()];
    } else if ((out & ring_->mask()) >= real_size) {
      // new_pos is in the next block
      data_ptr = &ring_->data_[0];
    } else {
      // not enough space
      return nullptr;
    }
    pending_packet_ = data_ptr;
    pending_packet_->size = size;
    return &pending_packet_->data[0];
  }

  /**
   * Commits the data to the buffer, so that it can be read out.
   */
  void Commit() {
    // only written from push thread
    const auto in = ring_->prod_tail_.load(std::memory_order_relaxed);

    pending_packet_->submitted = true;
    auto size = pending_packet_->size + sizeof(Packet);

    // wrap around the ring buffer
    if (&pending_packet_->data[0] == &ring_->data_[0]) {
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
  Umq *ring_;
  PacketPtr pending_packet_{nullptr};
};

class Consumer {
 public:
  explicit Consumer(Umq *queue) : ring_(queue) {}

  ~Consumer() = default;

  /**
   * Gets a pointer to the contiguous block in the buffer, and returns the size
   * of that block.
   * @param size returns the size of the contiguous block
   * @return pointer to the contiguous block
   */
  void *TryReadOnePacket(size_t *out_size) {
    const auto in = ring_->prod_tail_.load(std::memory_order_acquire);
    const auto last = ring_->prod_last_.load(std::memory_order_relaxed);
    const auto out = ring_->cons_head_.load(std::memory_order_relaxed);

    if (out == in) {
      return nullptr;
    }

    const auto cur_in = in & ring_->mask();
    const auto cur_out = out & ring_->mask();

    uint8_t *data_ptr = nullptr;
    // read and write are still in the same block
    if (cur_out < cur_in) {
      size_t const total_size = in - out;
      reading_packet_ =
          reinterpret_cast<Packet *>((void *)(&ring_->data_[cur_out]));
    }
    if (out != last) {
      // read and write are in different blocks, read the current remaining data
      size_t const total_size = last - out;
      return &ring_->data_[cur_out];
    }

    // The current block has been read, "write" has reached the next block
    const auto cur_block_start = in & ~ring_->mask();
    // Move the read index, which can make room for the write
    ring_->cons_head_.store(cur_block_start, std::memory_order_relaxed);

    if (cur_in == 0U) {
      return nullptr;
    }

    size_t const total_size = cur_in;
    return &ring_->data_[0];
  }

  /**
   * Releases data from the buffer, so that more data can be written in.
   * @param size the size of the data to be released
   *
   * Note:
   *
   * The validity of the size is not checked, it needs to be within the range
   * returned by the TryReadOnePacket function.
   *
   */
  void ReleasePacket(size_t size) {
    auto out = ring_->cons_head_.load(std::memory_order_relaxed);
    const auto cur_out = out & ring_->mask();
    std::memset(&ring_->data_[cur_out], 0, size);
    ring_->cons_head_.store(out + size, std::memory_order_release);
  }

 private:
  Umq *ring_;
  Packet *reading_packet_{nullptr};
};
}  // namespace umq
}  // namespace ulog
