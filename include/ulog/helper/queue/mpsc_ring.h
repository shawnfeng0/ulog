#pragma once

#include <ulog/ulog.h>
#include <unistd.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <thread>

#include "lite_notifier.h"
#include "power_of_two.h"

// The basic principle of circular queue implementation:
// A - B is the position of A relative to B

// Diagram example:
// 0--_____________0--_____________
//    ^in    ^new

// Explain:
// 0: The starting position of each range
// -: Positions already occupied by other producers
// _: Positions not occupied by producers

// Experience in lock-free queue design:
// 1. When using yield() and loop calls like the disruptor, when the number of competing threads exceeds the number of
// CPUs , using yield() may not necessarily schedule to the target thread, and there will be a serious performance
// degradation , even worse than the traditional locking method.The use of yield() should be minimized.
// 2. Using cache lines to fill the frequently used write_index and read_index variables can indeed increase performance
// by about 15 %.

namespace ulog {
namespace umq {

inline unsigned align8(const unsigned size) { return (size + 7) & ~7; }

class Producer;
class Consumer;

struct Packet {
  std::atomic_uint32_t size;
  uint8_t data[0];
} __attribute__((aligned(8)));

union PacketPtr {
  explicit PacketPtr(uint8_t *ptr = nullptr) : ptr_(ptr) {}

  PacketPtr &operator=(uint8_t *ptr) {
    ptr_ = ptr;
    return *this;
  }

  explicit operator bool() const noexcept { return ptr_ != nullptr; }
  auto operator->() const -> Packet * { return header; }
  auto get() const -> uint8_t * { return this->ptr_; }
  PacketPtr next() const { return PacketPtr(ptr_ + (sizeof(Packet) + align8(header->size))); }

 private:
  Packet *header;
  __attribute__((unused)) uint8_t *ptr_;
};

class Umq {
  friend class Producer;
  friend class Consumer;

 public:
  explicit Umq(size_t num_elements) : cons_head_(0), prod_head_(0), prod_last_(0) {
    if (num_elements < 2) num_elements = 2;

    // round up to the next power of 2, since our 'let the indices wrap'
    // technique works only in this case.
    num_elements = queue::RoundUpPowOfTwo(num_elements);

    mask_ = num_elements - 1;
    data_ = new unsigned char[num_elements];
    std::memset(data_, 0, sizeof(data_));
  }

  ~Umq() { delete[] data_; }

  void Debug() {
    const auto prod_head = prod_head_.load(std::memory_order_relaxed);
    const auto prod_last = prod_last_.load(std::memory_order_relaxed);
    const auto cons_head = cons_head_.load(std::memory_order_relaxed);

    const auto cur_p_head = prod_head & mask();
    const auto cur_p_last = prod_last & mask();
    const auto cur_c_head = cons_head & mask();

    printf("prod_head: %zd(%zd), prod_last: %zd(%zd), cons_head: %zd(%zd)\n", prod_head, cur_p_head, prod_last,
           cur_p_last, cons_head, cur_c_head);

    for (size_t i = 0; i < size(); i++) {
      printf("|%02x", data_[i]);
    }
    printf("|\n");

    for (size_t i = 0; i < size(); i++)
      printf("%s", i == cur_p_head ? ("^prod_head:" + std::to_string(prod_head)).c_str() : "   ");
    printf("\n");
    for (size_t i = 0; i < size(); i++)
      printf("%s", i == cur_p_last ? ("^prod_last:" + std::to_string(prod_last)).c_str() : "   ");
    printf("\n");
    for (size_t i = 0; i < size(); i++)
      printf("%s", i == cur_c_head ? ("^cons_head:" + std::to_string(cons_head)).c_str() : "   ");
    printf("\n");
  }

 private:
  size_t size() const { return mask_ + 1; }

  size_t mask() const { return mask_; }

  size_t next_buffer(const size_t index) const { return (index & ~mask()) + size(); }

  uint8_t *data_;  // the buffer holding the data
  size_t mask_;

  uint8_t pad0[64]{};  // Using cache line filling technology can improve performance by 15%
  std::atomic<size_t> cons_head_;

  uint8_t pad1[64]{};
  std::atomic<size_t> prod_head_;
  std::atomic<size_t> prod_last_;

  uint8_t pad2[64]{};
  LiteNotifier prod_notifier_;
  LiteNotifier cons_notifier_;
};

class Producer {
 public:
  explicit Producer(Umq *queue) : ring_(queue), packet_size_(0) {}

  ~Producer() = default;

  /**
   * Reserve space of size, automatically retry until timeout
   * @param size size of space to reserve
   * @param timeout_ms The maximum waiting time if there is insufficient space in the queue
   * @return data pointer if successful, otherwise nullptr
   */
  uint8_t *ReserveOrWait(const size_t size, const uint32_t timeout_ms) {
    uint8_t *ptr;
    ring_->cons_notifier_.wait_for(std::chrono::milliseconds(timeout_ms),
                                   [&] { return (ptr = Reserve(size)) != nullptr; });
    return ptr;
  }

  /**
   * Try to reserve space of size
   * @param size size of space to reserve
   * @return data pointer if successful, otherwise nullptr
   */
  uint8_t *Reserve(const size_t size) {
    const auto packet_size = sizeof(Packet) + align8(size);

    packet_head_ = ring_->prod_head_.load(std::memory_order_relaxed);
    do {
      const auto cons_head = ring_->cons_head_.load(std::memory_order_relaxed);
      packet_next_ = packet_head_ + packet_size;

      // Not enough space
      if (packet_next_ - cons_head > ring_->size()) {
        return nullptr;
      }

      const auto relate_pos = packet_next_ & ring_->mask();
      // Both new position and write_index are in the same range
      // 0--_____________________________0--_____________________________
      //    ^in               ^new
      // OR
      // 0--_____________________________0--_____________________________
      //    ^in                          ^new
      if (relate_pos >= packet_size || relate_pos == 0) {
        if (!ring_->prod_head_.compare_exchange_weak(packet_head_, packet_next_, std::memory_order_relaxed)) {
          continue;
        }

        // Whenever we wrap around, we update the last variable to ensure logical
        // consistency.
        if (relate_pos == 0) {
          ring_->prod_last_.store(packet_next_, std::memory_order_relaxed);
        }
        pending_packet_ = &ring_->data_[packet_head_ & ring_->mask()];
        break;
      }

      if ((cons_head & ring_->mask()) >= packet_size) {
        // new_pos is in the next block
        // 0__________------------------___0__________------------------___
        //            ^out              ^in
        packet_next_ = ring_->next_buffer(packet_head_) + packet_size;
        if (!ring_->prod_head_.compare_exchange_weak(packet_head_, packet_next_, std::memory_order_relaxed)) {
          continue;
        }
        ring_->prod_last_.store(packet_head_, std::memory_order_relaxed);
        pending_packet_ = &ring_->data_[0];
        break;
      }
      // Neither the end of the current range nor the head of the next range is enough
      return nullptr;
    } while (true);

    packet_size_ = size;
    return &pending_packet_->data[0];
  }

  /**
   * Commits the data to the buffer, so that it can be read out.
   */
  void Commit() {
    pending_packet_->size = packet_size_;
    // prod_tail cannot be modified here:
    // 1. If you wait for prod_tail to update to the current position, there will be a lot of performance loss
    // 2. If you don't wait for prod_tail, just do a check and mark? It doesn't work either. Because it is a wait-free
    // process, in a highly competitive scenario, the queue may have been updated once, and the data is unreliable.
    ring_->prod_notifier_.notify_when_blocking();
  }

 private:
  Umq *ring_;
  PacketPtr pending_packet_;
  size_t packet_size_;
  decltype(ring_->prod_head_.load()) packet_head_;
  decltype(ring_->prod_head_.load()) packet_next_;
};

class Consumer {
 public:
  explicit Consumer(Umq *queue) : ring_(queue) {}

  ~Consumer() = default;
  void Debug() { ring_->Debug(); }

  /**
   * Gets a pointer to the contiguous block in the buffer, and returns the size of that block. automatically retry until
   * timeout
   * @param packet_count returns the count of the packet
   * @param timeout_ms The maximum waiting time
   * @return pointer to the contiguous block
   */
  PacketPtr ReadOrWait(size_t *packet_count, const uint32_t timeout_ms) {
    PacketPtr ptr;
    ring_->prod_notifier_.wait_for(std::chrono::milliseconds(timeout_ms),
                                   [&] { return (ptr = TryRead(packet_count)).get() != nullptr; });
    return ptr;
  }

  /**
   * Gets a pointer to the contiguous block in the buffer, and returns the size of that block.
   * @param packet_count returns the count of the packet
   * @return pointer to the contiguous block
   */
  PacketPtr TryRead(size_t *packet_count) {
    const auto prod_head = ring_->prod_head_.load(std::memory_order_acquire);
    const auto cons_head = ring_->cons_head_.load(std::memory_order_relaxed);

    // no data
    if (cons_head == prod_head) {
      return PacketPtr{nullptr};
    }

    const auto cur_prod_tail = prod_head & ring_->mask();
    const auto cur_cons_head = cons_head & ring_->mask();

    // read and write are still in the same block
    // 0__________------------------___0__________------------------___
    //            ^cons_head        ^prod_tail
    if (cur_cons_head < cur_prod_tail) {
      return CheckRealSize(&ring_->data_[cur_cons_head], cur_prod_tail - cur_cons_head, packet_count);
    }

    // Due to the update order, prod_head will be updated first and prod_last will be updated later.
    // prod_head is already in the next set of loops, so you need to make sure prod_last is updated to the position
    // before prod_head.
    auto prod_last = ring_->prod_last_.load(std::memory_order_relaxed);
    while (prod_last - cons_head > ring_->size()) {
      std::this_thread::yield();
      prod_last = ring_->prod_last_.load(std::memory_order_relaxed);
    }

    // read and write are in different blocks, read the current remaining data
    // 0---_______________skip-skip-ski0---_______________skip-skip-ski
    //     ^prod_tail     ^cons_head       ^prod_tail
    //                    ^prod_last
    if (cons_head == prod_last && cur_cons_head != 0) {
      // The current block has been read, "write" has reached the next block
      // Move the read index, which can make room for the writer
      ring_->cons_head_.store(ring_->next_buffer(cons_head), std::memory_order_release);
      return CheckRealSize(&ring_->data_[0], cur_prod_tail, packet_count);
    }

    // 0---___------------skip-skip-ski0---___------------skip-skip-ski
    //        ^out        ^last            ^in
    return CheckRealSize(&ring_->data_[cur_cons_head], prod_last - cons_head, packet_count);
  }

  /**
   * Releases data from the buffer, so that more data can be written in.
   *
   * The validity of the size is not checked, it needs to be within the range
   * returned by the TryReadOnePacket function.
   */
  void ReleasePacket() {
    std::memset(packet_head_, 0, packet_total_size_);
    ring_->cons_head_.fetch_add(packet_total_size_, std::memory_order_release);
    ring_->cons_notifier_.notify_when_blocking();
  }

 private:
  PacketPtr CheckRealSize(uint8_t *data, const size_t size, size_t *packet_count) {
    PacketPtr pk;
    size_t count = 0;
    for (pk = data; pk.get() < data + size; pk = pk.next()) {
      if (pk->size != 0)
        count++;
      else
        break;
    }

    packet_total_size_ = pk.get() - data;
    packet_head_ = data;

    if (count == 0) return PacketPtr(nullptr);

    *packet_count = count;
    return PacketPtr(data);
  }

  Umq *ring_;
  size_t packet_total_size_{0};
  uint8_t *packet_head_{nullptr};
};
}  // namespace umq
}  // namespace ulog
