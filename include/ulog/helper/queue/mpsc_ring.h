#pragma once

#include <ulog/ulog.h>
#include <unistd.h>

#include <atomic>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <thread>

#include "lite_notifier.h"

// The basic principle of circular queue implementation:
// A - B is the position of A relative to B

// Diagram example:
// 0--_____________0--_____________
//    ^in    ^new

// Explain:
// 0: The starting position of each range
// -: Positions already occupied by other producers
// _: Positions not occupied by producers

namespace ulog {
namespace umq {

inline unsigned align8(const unsigned size) { return (size + 7) & ~7; }

template <int buffer_size>
class Producer;

template <int buffer_size>
class Consumer;

struct Packet {
  static constexpr size_t kSizeMask = (1 << 30) - 1;
  static constexpr size_t kMaxDataSize = kSizeMask;

  void set_size(const uint32_t size) { attr.store(size, std::memory_order_relaxed); }
  uint32_t size() const { return attr.load(std::memory_order_relaxed) & kSizeMask; }

  void mark_discarded() { attr.fetch_or(1 << 30, std::memory_order_release); }
  bool discarded() const { return attr.load(std::memory_order_relaxed) & (1 << 30); }

  void mark_submitted() { attr.fetch_or(1 << 31, std::memory_order_release); }
  bool submitted() const { return attr.load(std::memory_order_relaxed) & (1 << 31); }

  uint32_t magic;
  std::atomic_uint32_t attr;
  uint8_t data[0];
} __attribute__((aligned(8)));

union PacketPtr {
  explicit PacketPtr(void *ptr) : ptr_(ptr) {}

  explicit operator bool() const noexcept { return ptr_ != nullptr; }
  auto operator->() const -> Packet * { return header; }

  auto get() const -> void * { return this->ptr_; }
  PacketPtr &operator=(void *ptr) {
    ptr_ = ptr;
    return *this;
  }

 private:
  Packet *header;
  __attribute__((unused)) void *ptr_;
};

template <int buffer_size>
class Umq {
  friend class Producer<buffer_size>;
  friend class Consumer<buffer_size>;

 public:
  explicit Umq(/*size_t num_elements*/) : cons_head_(0), prod_head_(0), prod_last_(0) {
    // if (num_elements < 2) {
    // num_elements = 2;
    // } else {
    // round up to the next power of 2, since our 'let the indices wrap'
    // technique works only in this case.
    // num_elements = queue::RoundUpPowOfTwo(num_elements);
    // }
    // mask_ = num_elements - 1;
    // data_ = std::make_unique<uint8_t[]>(mask_ + 1);
    std::memset(data_, 0, sizeof(data_));
  }

  ~Umq() = default;

  void Debug() {
    const auto prod_head = prod_head_.load(std::memory_order_acquire);
    const auto prod_tail = prod_tail_.load(std::memory_order_acquire);
    const auto prod_last = prod_last_.load(std::memory_order_relaxed);
    const auto cons_head = cons_head_.load(std::memory_order_relaxed);

    const auto cur_p_head = prod_head & mask();
    const auto cur_p_tail = prod_tail & mask();
    const auto cur_p_last = prod_last & mask();
    const auto cur_c_head = cons_head & mask();

    printf("prod_head: %zd(%zd), prod_tail: %zd(%zd), prod_last: %zd(%zd), cons_head: %zd(%zd)\n", prod_head,
           cur_p_head, prod_tail, cur_p_tail, prod_last, cur_p_last, cons_head, cur_c_head);

    for (size_t i = 0; i < buffer_size; i++) {
      printf("|%02x", data_[i]);
    }
    printf("|\n");

    for (size_t i = 0; i < buffer_size; i++)
      printf("%s", i == cur_p_head ? ("^prod_head:" + std::to_string(prod_head)).c_str() : "   ");
    printf("\n");
    for (size_t i = 0; i < buffer_size; i++)
      printf("%s", i == cur_p_head ? ("^prod_tail:" + std::to_string(prod_tail)).c_str() : "   ");
    printf("\n");
    for (size_t i = 0; i < buffer_size; i++)
      printf("%s", i == cur_p_last ? ("^prod_last:" + std::to_string(prod_last)).c_str() : "   ");
    printf("\n");
    for (size_t i = 0; i < buffer_size; i++)
      printf("%s", i == cur_c_head ? ("^cons_head:" + std::to_string(cons_head)).c_str() : "   ");
    printf("\n");
  }

 private:
  size_t size() const { return mask_ + 1; }

  size_t mask() const { return mask_; }

  size_t next_buffer(const size_t index) const { return (index & ~mask()) + size(); }

  // std::unique_ptr<uint8_t[]> data_;  // the buffer holding the data
  uint8_t data_[buffer_size]{};  // the buffer holding the data
  size_t mask_ = buffer_size - 1;

  std::atomic<size_t> cons_head_;

  std::atomic<size_t> prod_tail_{};
  std::atomic<size_t> prod_head_;
  std::atomic<size_t> prod_last_;

  LiteNotifier prod_notifier_;
  LiteNotifier cons_notifier_;
};

template <int buffer_size>
class Producer {
 public:
  explicit Producer(Umq<buffer_size> *queue) : ring_(queue) {}

  ~Producer() = default;

  /**
   * Reserve space of size, automatically retry until timeout
   * @param size size of space to reserve
   * @param timeout_ms The maximum waiting time if there is insufficient space in the queue
   * @return data pointer if successful, otherwise nullptr
   */
  void *ReserveOrWait(const size_t size, const uint32_t timeout_ms) {
    void *ptr;
    ring_->cons_notifier_.wait_for(std::chrono::milliseconds(timeout_ms),
                                   [&] { return (ptr = Reserve(size)) != nullptr; });
    return ptr;
  }

  /**
   * Try to reserve space of size
   * @param size size of space to reserve
   * @return data pointer if successful, otherwise nullptr
   */
  void *Reserve(const size_t size) {
    const auto packet_size = sizeof(Packet) + align8(size);

    head_ = ring_->prod_head_.load(std::memory_order_relaxed);
    do {
      const auto cons_head = ring_->cons_head_.load(std::memory_order_relaxed);
      next_ = head_ + packet_size;

      // Not enough space
      if (next_ - cons_head > ring_->size()) {
        return nullptr;
      }

      const auto relate_pos = next_ & ring_->mask();
      // Both new position and write_index are in the same range
      // 0--_____________________________0--_____________________________
      //    ^in               ^new
      // OR
      // 0--_____________________________0--_____________________________
      //    ^in                          ^new
      if (relate_pos >= packet_size || relate_pos == 0) {
        if (!ring_->prod_head_.compare_exchange_weak(head_, next_, std::memory_order_release)) {
          continue;
        }

        // Whenever we wrap around, we update the last variable to ensure logical
        // consistency.
        if (relate_pos == 0) {
          ring_->prod_last_.store(next_, std::memory_order_release);
        }
        pending_packet_ = &ring_->data_[head_ & ring_->mask()];
        break;
      }

      if ((cons_head & ring_->mask()) >= packet_size) {
        // new_pos is in the next block
        // 0__________------------------___0__________------------------___
        //            ^out              ^in
        // packet: ------
        next_ = ring_->next_buffer(head_) + packet_size;
        if (!ring_->prod_head_.compare_exchange_weak(head_, next_, std::memory_order_relaxed)) {
          continue;
        }
        ring_->prod_last_.store(head_, std::memory_order_release);
        pending_packet_ = &ring_->data_[0];
        break;
      }
      // Neither the end of the current range nor the head of the next range is enough
      return nullptr;
    } while (true);

    pending_packet_->magic = 0xefbeefbe;
    pending_packet_->set_size(size);
    return &pending_packet_->data[0];
  }

  /**
   * Commits the data to the buffer, so that it can be read out.
   */
  void Commit() {
    if (!pending_packet_) return;
    pending_packet_->mark_submitted();
    pending_packet_ = nullptr;

    // Try to commit the data, if the commit fails, yield the CPU to other producer and try again
    int retry = 256;
    while (ring_->prod_tail_.load(std::memory_order_relaxed) != head_) {
      std::this_thread::yield();
    }

    // Retry failed, wait for other producers commit
    ring_->prod_notifier_.wait([&] { return ring_->prod_tail_.load(std::memory_order_relaxed) == head_; });
    ring_->prod_tail_.store(next_, std::memory_order_release);

    // Notify the consumer that there is data to read or the producer has committed
    ring_->prod_notifier_.notify_when_blocking();
  }

 private:
  Umq<buffer_size> *ring_;
  PacketPtr pending_packet_{nullptr};
  decltype(ring_->prod_head_.load()) head_;
  decltype(ring_->prod_head_.load()) next_;
};

template <int buffer_size>
class Consumer {
 public:
  explicit Consumer(Umq<buffer_size> *queue) : ring_(queue) {}

  ~Consumer() = default;
  void Debug() { ring_->Debug(); }

  /**
   * Gets a pointer to the contiguous block in the buffer, and returns the size of that block. automatically retry until
   * timeout
   * @param out_size returns the size of the contiguous block
   * @param timeout_ms The maximum waiting time
   * @return pointer to the contiguous block
   */
  void *ReadOrWait(uint32_t *out_size, const uint32_t timeout_ms) {
    void *ptr;
    ring_->prod_notifier_.wait_for(std::chrono::milliseconds(timeout_ms),
                                   [&] { return (ptr = TryRead(out_size)) != nullptr; });
    return ptr;
  }

  /**
   * Gets a pointer to the contiguous block in the buffer, and returns the size of that block.
   * @param out_size returns the size of the contiguous block
   * @return pointer to the contiguous block
   */
  void *TryRead(uint32_t *out_size) {
    const auto prod_tail = ring_->prod_tail_.load(std::memory_order_acquire);
    const auto prod_last = ring_->prod_last_.load(std::memory_order_relaxed);
    const auto cons_head = ring_->cons_head_.load(std::memory_order_relaxed);

    // no data
    if (cons_head == prod_tail) {
      return nullptr;
    }

    const auto cur_prod_tail = prod_tail & ring_->mask();
    const auto cur_cons_head = cons_head & ring_->mask();

    // read and write are still in the same block
    // 0__________------------------___0__________------------------___
    //            ^cons_head        ^prod_tail
    if (cur_cons_head < cur_prod_tail) {
      *out_size = cur_size_ = cur_prod_tail - cur_cons_head;
      return &ring_->data_[cur_cons_head];
    }

    // read and write are in different blocks, read the current remaining data
    // 0---_______________skip-skip-ski0---_______________skip-skip-ski
    //     ^prod_tail     ^cons_head       ^prod_tail
    //                    ^prod_last
    if (cons_head == prod_last && cur_cons_head != 0) {
      // The current block has been read, "write" has reached the next block
      // Move the read index, which can make room for the writer
      ring_->cons_head_.store(ring_->next_buffer(cons_head), std::memory_order_release);

      *out_size = cur_size_ = cur_prod_tail;
      return &ring_->data_[0];
    }

    // 0---___------------skip-skip-ski0---___------------skip-skip-ski
    //        ^out        ^last            ^in
    //
    // if (prod_last < cons_head) {
    //   ring_->Debug();
    // }
    *out_size = cur_size_ = prod_last - cons_head;
    return &ring_->data_[0];
  }

  /**
   * Releases data from the buffer, so that more data can be written in.
   *
   * The validity of the size is not checked, it needs to be within the range
   * returned by the TryReadOnePacket function.
   */
  void ReleasePacket() {
    ring_->cons_head_.fetch_add(cur_size_, std::memory_order_release);
    ring_->cons_notifier_.notify_when_blocking();
  }

 private:
  Umq<buffer_size> *ring_;
  size_t cur_size_{0};
};
}  // namespace umq
}  // namespace ulog
