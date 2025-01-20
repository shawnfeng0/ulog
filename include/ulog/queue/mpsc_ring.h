#pragma once

#include <unistd.h>

#include <atomic>
#include <cassert>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <thread>

#include "lite_notifier.h"
#include "memory_logger.h"
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
namespace mpsc {

inline unsigned align8(const unsigned size) { return (size + 7) & ~7; }

class Producer;
class Consumer;

struct Header {
  static constexpr size_t kFlagMask = 1U << 31;
  static constexpr size_t kSizeMask = kFlagMask - 1;

  void set_size(const uint32_t size, const std::memory_order m) { data_size.store(size, m); }
  uint32_t size(const std::memory_order m) const { return data_size.load(m) & kSizeMask; }

  void mark_discarded(const std::memory_order m) { data_size.fetch_or(kFlagMask, m); }
  bool discarded(const std::memory_order m) const { return data_size.load(m) & kFlagMask; }
  bool valid(const std::memory_order m) const { return data_size.load(m) != 0; }

  std::atomic_uint32_t reverse_size;

 private:
  std::atomic_uint32_t data_size{0};

 public:
  uint8_t data[0];
} __attribute__((aligned(8)));

union HeaderPtr {
  explicit HeaderPtr(void *ptr = nullptr) : ptr_(static_cast<uint8_t *>(ptr)) {}

  HeaderPtr &operator=(uint8_t *ptr) {
    ptr_ = ptr;
    return *this;
  }

  explicit operator bool() const noexcept { return ptr_ != nullptr; }
  auto operator->() const -> Header * { return header; }
  auto get() const -> uint8_t * { return this->ptr_; }
  HeaderPtr next() const { return HeaderPtr(ptr_ + (sizeof(Header) + align8(header->reverse_size))); }

 private:
  Header *header;
  __attribute__((unused)) uint8_t *ptr_;
};

class PacketGroup {
 public:
  explicit PacketGroup(const HeaderPtr packet_head = HeaderPtr{nullptr}, const size_t count = 0,
                       const size_t total_size = 0)
      : packet_count_(count), packet_head_(packet_head), group_ptr_(packet_head.get()), group_size_(total_size) {}

  explicit operator bool() const noexcept { return remain() > 0; }
  size_t remain() const { return packet_count_; }

  queue::Packet<> next() {
    if (!remain()) return queue::Packet<>{};

    const queue::Packet<> packet{packet_head_->size(std::memory_order_relaxed), packet_head_->data};
    packet_head_ = packet_head_.next();
    packet_count_--;
    return packet;
  }

  uint8_t *raw_ptr() const { return static_cast<uint8_t *>(group_ptr_); }
  size_t raw_size() const { return group_size_; }

 private:
  size_t packet_count_;
  HeaderPtr packet_head_;
  void *group_ptr_;
  size_t group_size_;
};

class DataPacket {
  friend class Consumer;

 public:
  explicit DataPacket(const PacketGroup &group0 = PacketGroup{}, const PacketGroup &group1 = PacketGroup{})
      : group0_(group0), group1_(group1) {}

  explicit operator bool() const noexcept { return remain() > 0; }
  size_t remain() const { return group0_.remain() + group1_.remain(); }

  queue::Packet<> next() {
    if (group0_.remain()) return group0_.next();
    if (group1_.remain()) return group1_.next();
    return queue::Packet<>{0, nullptr};
  }

 private:
  PacketGroup group0_;
  PacketGroup group1_;
};

class Mq : public std::enable_shared_from_this<Mq> {
  friend class Producer;
  friend class Consumer;

  struct Private {
    explicit Private() = default;
  };

 public:
  explicit Mq(size_t num_elements, Private) : cons_head_(0), prod_head_(0), prod_last_(0) {
    if (num_elements < 2) num_elements = 2;

    // round up to the next power of 2, since our 'let the indices wrap'
    // technique works only in this case.
    num_elements = queue::RoundUpPowOfTwo(num_elements);

    mask_ = num_elements - 1;
    data_ = new unsigned char[num_elements];
    std::memset(data_, 0, num_elements);
  }
  ~Mq() { delete[] data_; }

  // Everyone else has to use this factory function
  static std::shared_ptr<Mq> Create(size_t num_elements) { return std::make_shared<Mq>(num_elements, Private()); }
  using Producer = mpsc::Producer;
  using Consumer = mpsc::Consumer;

  /**
   * Ensure that all currently written data has been read and processed
   * @param wait_time The maximum waiting time
   */
  void Flush(const std::chrono::milliseconds wait_time = std::chrono::milliseconds(1000)) {
    prod_notifier_.notify_when_blocking();
    const auto prod_head = prod_head_.load();
    cons_notifier_.wait_for(wait_time, [&]() { return queue::IsPassed(prod_head, cons_head_.load()); });
  }

  /**
   * Notify all waiting threads, so that they can check the status of the queue
   */
  void Notify() {
    prod_notifier_.notify_when_blocking();
    cons_notifier_.notify_when_blocking();
  }

 private:
  size_t size() const { return mask_ + 1; }

  size_t mask() const { return mask_; }

  size_t next_buffer(const size_t index) const { return (index & ~mask()) + size(); }

  uint8_t *data_;  // the buffer holding the data
  size_t mask_;

  [[maybe_unused]] uint8_t pad0[64]{};  // Using cache line filling technology can improve performance by 15%
  std::atomic<uint32_t> cons_head_;

  [[maybe_unused]] uint8_t pad1[64]{};
  std::atomic<uint32_t> prod_head_;
  std::atomic<uint32_t> prod_last_;

  [[maybe_unused]] uint8_t pad2[64]{};
  LiteNotifier prod_notifier_;
  LiteNotifier cons_notifier_;
};

class Producer {
 public:
  explicit Producer(const std::shared_ptr<Mq> &ring) : ring_(ring) {}
  ~Producer() = default;

  /**
   * Reserve space of size, automatically retry until timeout
   * @param size size of space to reserve
   * @param timeout The maximum waiting time if there is insufficient space in the queue
   * @return data pointer if successful, otherwise nullptr
   */
  uint8_t *ReserveOrWaitFor(const size_t size, const std::chrono::milliseconds timeout) {
    uint8_t *ptr;
    ring_->cons_notifier_.wait_for(timeout, [&] { return (ptr = Reserve(size)) != nullptr; });
    return ptr;
  }

  uint8_t *ReserveOrWait(const size_t size) {
    uint8_t *ptr;
    ring_->cons_notifier_.wait([&] { return (ptr = Reserve(size)) != nullptr; });
    return ptr;
  }

  /**
   * Try to reserve space of size
   * @param size size of space to reserve
   * @return data pointer if successful, otherwise nullptr
   */
  uint8_t *Reserve(const size_t size) {
    const auto packet_size = sizeof(Header) + align8(size);
    HeaderPtr pending_packet_;

    auto packet_head_ = ring_->prod_head_.load(std::memory_order_relaxed);
    do {
      const auto cons_head = ring_->cons_head_.load(std::memory_order_acquire);
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
        // After being fully read out, it will be marked as all 0 by the reader
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

    pending_packet_->reverse_size.store(size, std::memory_order_relaxed);
    return &pending_packet_->data[0];
  }

  /**
   * Commits the data to the buffer, so that it can be read out.
   */
  void Commit(const uint8_t *data, const size_t real_size) {
    const HeaderPtr pending_packet_(intrusive::owner_of(data, &Header::data));
    assert(real_size <= pending_packet_->reverse_size);

    if (real_size) {
      pending_packet_->set_size(real_size, std::memory_order_release);
    } else {
      pending_packet_->mark_discarded(std::memory_order_relaxed);
    }

    // prod_tail cannot be modified here:
    // 1. If you wait for prod_tail to update to the current position, there will be a lot of performance loss
    // 2. If you don't wait for prod_tail, just do a check and mark? It doesn't work either. Because it is a wait-free
    // process, in a highly competitive scenario, the queue may have been updated once, and the data is unreliable.
    ring_->prod_notifier_.notify_when_blocking();
  }

  /**
   * Ensure that all currently written data has been read and processed
   * @param wait_time The maximum waiting time
   */
  void Flush(const std::chrono::milliseconds wait_time = std::chrono::milliseconds(1000)) const {
    ring_->cons_notifier_.wait_for(wait_time,
                                   [&]() { return queue::IsPassed(packet_next_, ring_->cons_head_.load()); });
  }

 private:
  std::shared_ptr<Mq> ring_;
  decltype(ring_->prod_head_.load()) packet_next_{};
};

class Consumer {
 public:
  explicit Consumer(const std::shared_ptr<Mq> &ring) : ring_(ring) {}

  ~Consumer() = default;

  /**
   * Gets a pointer to the contiguous block in the buffer, and returns the size of that block. automatically retry until
   * timeout
   * @param timeout The maximum waiting time
   * @param other_condition Other wake-up conditions
   * @return pointer to the contiguous block
   */
  template <typename Condition>
  DataPacket ReadOrWait(const std::chrono::milliseconds timeout, Condition other_condition) {
    DataPacket ptr;
    ring_->prod_notifier_.wait_for(timeout, [&] { return (ptr = Read()).remain() > 0 || other_condition(); });
    return ptr;
  }
  DataPacket ReadOrWait(const std::chrono::milliseconds timeout) {
    return ReadOrWait(timeout, [] { return false; });
  }
  template <typename Condition>
  DataPacket ReadOrWait(Condition other_condition) {
    DataPacket ptr;
    ring_->prod_notifier_.wait([&] { return (ptr = Read()).remain() > 0 || other_condition(); });
    return ptr;
  }

  /**
   * Gets a pointer to the contiguous block in the buffer, and returns the size of that block.
   * @return pointer to the contiguous block
   */
  DataPacket Read() {
    cons_head = ring_->cons_head_.load(std::memory_order_relaxed);
    const auto prod_head = ring_->prod_head_.load(std::memory_order_acquire);

    // no data
    if (cons_head == prod_head) {
      return DataPacket{};
    }

    const auto cur_prod_head = prod_head & ring_->mask();
    const auto cur_cons_head = cons_head & ring_->mask();

    // read and write are still in the same block
    // 0__________------------------___0__________------------------___
    //            ^cons_head        ^prod_head
    if (cur_cons_head < cur_prod_head) {
      const auto group = CheckRealSize(&ring_->data_[cur_cons_head], cur_prod_head - cur_cons_head);
      if (!group) return DataPacket{};

      cons_head_next = cons_head + group.raw_size();

      return DataPacket{group};
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
    //     ^prod_head     ^cons_head       ^prod_head
    //                    ^prod_last
    if (cons_head == prod_last) {
      // The current block has been read, "write" has reached the next block
      // Move the read index, which can make room for the writer
      const auto group = CheckRealSize(&ring_->data_[0], cur_prod_head);
      if (!group) return DataPacket{};

      if (cur_cons_head == 0) {
        cons_head_next = cons_head + group.raw_size();
      } else {
        cons_head_next = ring_->next_buffer(cons_head) + group.raw_size();
      }

      return DataPacket{group};
    }

    // 0---___------------skip-skip-ski0---___------------skip-skip-ski
    //        ^cons_head  ^prod_last       ^prod_head
    const size_t expected_size = prod_last - cons_head;
    const auto group0 = CheckRealSize(&ring_->data_[cur_cons_head], expected_size);
    if (!group0) return DataPacket{};

    // The current packet group has been read, continue reading the next packet group
    if (expected_size == group0.raw_size()) {
      // Read the next group only if the current group has been committed
      const auto group1 = CheckRealSize(&ring_->data_[0], cur_prod_head);
      cons_head_next = ring_->next_buffer(cons_head) + group1.raw_size();

      return DataPacket{group0, group1};
    }

    cons_head_next = cons_head + group0.raw_size();

    return DataPacket{group0};
  }

  /**
   * Releases data from the buffer, so that more data can be written in.
   */
  void Release(const DataPacket &data) const {
    for (auto &group : {data.group0_, data.group1_}) {
      if (!group.raw_size()) continue;

      std::memset(group.raw_ptr(), 0, group.raw_size());
    }

    ring_->cons_head_.store(cons_head_next, std::memory_order_release);
    ring_->cons_notifier_.notify_when_blocking();
  }

 private:
  static PacketGroup CheckRealSize(uint8_t *data, const size_t size, const size_t max_packet_count = 1024) {
    HeaderPtr pk;
    size_t count = 0;
    for (pk = data; pk.get() < data + size;) {
      if (pk->valid(std::memory_order_relaxed) == 0) break;

      count++;
      pk = pk.next();

      if (count >= max_packet_count) break;
    }

    if (count == 0) return PacketGroup{};

    return PacketGroup(HeaderPtr(data), count, pk.get() - data);
  }
  size_t cons_head_next = 0;
  size_t cons_head = 0;
  std::shared_ptr<Mq> ring_;
};
}  // namespace mpsc
}  // namespace ulog
