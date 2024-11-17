#pragma once

#include <unistd.h>

#include <atomic>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

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
  explicit Umq(/*size_t num_elements*/) : cons_head_(0), prod_tail_(0), prod_last_(0) {
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
    const auto in = prod_tail_.load(std::memory_order_acquire);
    const auto last = prod_last_.load(std::memory_order_relaxed);
    const auto out = cons_head_.load(std::memory_order_relaxed);

    const auto cur_in = in & mask();
    const auto cur_last = last & mask();
    const auto cur_out = out & mask();

    printf("in: %zd(%zd), last: %zd(%zd), out: %zd(%zd)\n", in, cur_in, last, cur_last, out, cur_out);
    for (size_t i = 0; i < buffer_size; i++) {
      printf("|%02x", data_[i]);
    }
    printf("|\n");

    for (size_t i = 0; i < buffer_size; i++) printf("%s", i == cur_in ? "^in" : "   ");
    printf("\n");
    for (size_t i = 0; i < buffer_size; i++) printf("%s", i == cur_last ? "^last" : "   ");
    printf("\n");
    for (size_t i = 0; i < buffer_size; i++) printf("%s", i == cur_out ? "^out" : "   ");
    printf("\n");
  }

 private:
  size_t size() const { return mask_ + 1; }

  size_t mask() const { return mask_; }

  size_t next_buffer(const size_t index) const { return (index & ~mask()) + size(); }

  // std::unique_ptr<uint8_t[]> data_;  // the buffer holding the data
  uint8_t data_[buffer_size];  // the buffer holding the data
  size_t mask_ = buffer_size - 1;

  std::atomic<size_t> cons_head_;

  std::atomic<size_t> prod_tail_;
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

    auto in = ring_->prod_tail_.load(std::memory_order_relaxed);
    do {
      const auto out = ring_->cons_head_.load(std::memory_order_relaxed);
      const auto pos = in + packet_size;

      // Not enough space
      if (pos - out > ring_->size()) {
        return nullptr;
      }

      const auto relate_pos = pos & ring_->mask();
      // Both new position and write_index are in the same range
      // 0--_____________________________0--_____________________________
      //    ^in               ^new
      // OR
      // 0--_____________________________0--_____________________________
      //    ^in                          ^new
      if (relate_pos >= packet_size || relate_pos == 0) {
        if (!ring_->prod_tail_.compare_exchange_weak(in, pos, std::memory_order_release)) {
          continue;
        }

        // Whenever we wrap around, we update the last variable to ensure logical
        // consistency.
        if (relate_pos == 0) {
          ring_->prod_last_.store(pos, std::memory_order_release);
        }
        pending_packet_ = &ring_->data_[in & ring_->mask()];
        break;
      }

      if ((out & ring_->mask()) >= packet_size) {
        // new_pos is in the next block
        // 0__________------------------___0__________------------------___
        //            ^out              ^in
        // packet: ------
        if (!ring_->prod_tail_.compare_exchange_weak(in, ring_->next_buffer(in) + packet_size,
                                                     std::memory_order_relaxed)) {
          continue;
        }
        ring_->prod_last_.store(in, std::memory_order_release);
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
    ring_->prod_notifier_.notify_when_blocking();
  }

 private:
  Umq<buffer_size> *ring_;
  PacketPtr pending_packet_{nullptr};
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
                                   [&] { return (ptr = TryReadOnePacket(out_size)) != nullptr; });
    return ptr;
  }

  /**
   * Gets a pointer to the contiguous block in the buffer, and returns the size of that block.
   * @param out_size returns the size of the contiguous block
   * @return pointer to the contiguous block
   */
  void *TryReadOnePacket(uint32_t *out_size) {
    const auto in = ring_->prod_tail_.load(std::memory_order_acquire);
    const auto last = ring_->prod_last_.load(std::memory_order_relaxed);
    const auto out = ring_->cons_head_.load(std::memory_order_relaxed);

    // no data
    if (out == in) {
      return nullptr;
    }

    const auto cur_in = in & ring_->mask();
    const auto cur_out = out & ring_->mask();

    // read and write are still in the same block
    // 0__________------------------___0__________------------------___
    //            ^cur_out          ^cur_in
    if (cur_out < cur_in) {
      return CheckPacket(&ring_->data_[cur_out], out_size);
    }

    // read and write are in different blocks, read the current remaining data
    // 0---_______________skip-skip-ski0---_______________skip-skip-ski
    //     ^cur_in        ^out             ^in
    //                    ^last
    // OR
    // 0-------------------------------0-------------------------------
    // ^out                            ^in
    // ^last
    if (out == last && cur_out != 0) {
      // The current block has been read, "write" has reached the next block
      // Move the read index, which can make room for the writer
      ring_->cons_head_.store(ring_->next_buffer(out), std::memory_order_release);

      if (cur_in == 0) {  // TODO: impossible, need delete
        // 0__________________skip-skip-ski0__________________skip-skip-ski
        // ^cur_in            ^out         ^in
        //                    ^last
        printf("Maybe bug, in: %" PRIu64 "\n", in);
        return nullptr;
      }

      return CheckPacket(&ring_->data_[0], out_size);
    }
    // 0---___------------skip-skip-ski0---___------------skip-skip-ski
    //        ^out        ^last            ^in
    //
    return CheckPacket(&ring_->data_[cur_out], out_size);
  }

  /**
   * Releases data from the buffer, so that more data can be written in.
   *
   * The validity of the size is not checked, it needs to be within the range
   * returned by the TryReadOnePacket function.
   */
  void ReleasePacket() { ReleasePacket(reading_packet_); }

 private:
  void ReleasePacket(const PacketPtr &reading_packet) {
    const auto out = ring_->cons_head_.load(std::memory_order_relaxed);
    const auto last = ring_->prod_last_.load(std::memory_order_relaxed);

    const auto packet_size = sizeof(Packet) + align8(reading_packet->size());
    std::memset(reading_packet.get(), 0, packet_size);

    if (out + packet_size == last) {
      ring_->cons_head_.store(ring_->next_buffer(out), std::memory_order_release);
    } else {
      ring_->cons_head_.store(out + packet_size, std::memory_order_release);
    }
    ring_->cons_notifier_.notify_when_blocking();
  }

  void *CheckPacket(void *ptr, uint32_t *out_size) {
    const PacketPtr reading_packet(ptr);
    if (reading_packet->discarded()) {
      ReleasePacket(reading_packet);
      return nullptr;
    }

    if (!reading_packet->submitted()) {
      return nullptr;
    }

    reading_packet_ = reading_packet;
    *out_size = reading_packet_->size();
    return &reading_packet_->data[0];
  }

  Umq<buffer_size> *ring_;
  PacketPtr reading_packet_{nullptr};
};
}  // namespace umq
}  // namespace ulog
