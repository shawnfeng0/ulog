#pragma once

#include <atomic>
#include <cstddef>
#include <utility>

#include "lite_notifier.h"
#include "power_of_two.h"

namespace ulog {
namespace spsc {

template <typename T>
class Producer;
template <typename T>
class Consumer;
template <typename T>
class Mq;

/**
 * Data structure used when reading data externally
 */
template <typename T>
struct Packet {
  explicit Packet(const size_t s = 0, T *d = nullptr) : size(s), data(d) {}
  Packet(const Packet &other) = default;
  Packet &operator=(const Packet &other) = default;

  Packet(Packet &&other) noexcept : size(other.size), data(other.data) {
    other.data = nullptr;
    other.size = 0;
  }

  explicit operator bool() const noexcept { return data != nullptr; }
  size_t size = 0;
  T *data = nullptr;
};

template <typename T>
class DataPacket {
  friend class Mq<T>;
  friend class Consumer<T>;

 public:
  explicit DataPacket(const uint32_t end_index = 0, Packet<T> group0 = Packet<T>{}, Packet<T> group1 = Packet<T>{})
      : end_index_(end_index), group0_(std::move(group0)), group1_(std::move(group1)) {}

  explicit operator bool() const noexcept { return remain() > 0; }
  size_t remain() const { return (group0_ ? 1 : 0) + (group1_ ? 1 : 0); }

  Packet<T> next() {
    if (group0_) return std::move(group0_);
    if (group1_) return std::move(group1_);
    return Packet<T>{0, nullptr};
  }

 private:
  uint32_t end_index_;
  Packet<T> group0_;
  Packet<T> group1_;
};

template <typename T = char>
class Mq : public std::enable_shared_from_this<Mq<T>> {
  friend class Producer<T>;
  friend class Consumer<T>;

  struct Private {
    explicit Private() = default;
  };

 public:
  explicit Mq(size_t num_elements, Private) : out_(0), in_(0), last_(0) {
    if (num_elements < 2)
      num_elements = 2;
    else {
      // round up to the next power of 2, since our 'let the indices wrap'
      // technique works only in this case.
      num_elements = queue::RoundUpPowOfTwo(num_elements);
    }
    mask_ = num_elements - 1;
    data_ = new T[num_elements];
  }
  ~Mq() { delete[] data_; }

  // Everyone else has to use this factory function
  static std::shared_ptr<Mq> Create(size_t num_elements) { return std::make_shared<Mq>(num_elements, Private()); }
  using Producer = spsc::Producer<T>;
  using Consumer = spsc::Consumer<T>;

  /**
   * Ensure that all currently written data has been read and processed
   * @param wait_time The maximum waiting time
   */
  void Flush(const std::chrono::milliseconds wait_time = std::chrono::milliseconds(1000)) {
    prod_notifier_.notify_when_blocking();
    const auto prod_head = in_.load();
    cons_notifier_.wait_for(wait_time, [&]() { return queue::IsPassed(prod_head, out_.load()); });
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

  T *data_;
  size_t mask_;

  char pad[64]{};

  // for reader thread
  std::atomic_uint32_t out_;

  char pad1[64]{};

  // for writer thread
  std::atomic_uint32_t in_;
  std::atomic_uint32_t last_;

  char pad2[64]{};
  LiteNotifier prod_notifier_;
  LiteNotifier cons_notifier_;
};

template <typename T>
class Producer {
 public:
  explicit Producer(const std::shared_ptr<Mq<T>> &ring) : ring_(ring), wrapped_(false) {}

  /**
   * Reserve space of size, automatically retry until timeout
   * @param size size of space to reserve
   * @param timeout The maximum waiting time if there is insufficient space in the queue
   * @return data pointer if successful, otherwise nullptr
   */
  T *ReserveOrWaitFor(const size_t size, const std::chrono::milliseconds timeout) {
    T *ptr;
    ring_->cons_notifier_.wait_for(timeout, [&] { return (ptr = Reserve(size)) != nullptr; });
    return ptr;
  }

  T *ReserveOrWait(const size_t size) {
    T *ptr;
    ring_->cons_notifier_.wait([&] { return (ptr = Reserve(size)) != nullptr; });
    return ptr;
  }

  /**
   * Try to reserve space of size
   * @param size size of space to reserve
   * @return data pointer if successful, otherwise nullptr
   */
  T *Reserve(const size_t size) {
    const auto out = ring_->out_.load(std::memory_order_relaxed);
    const auto in = ring_->in_.load(std::memory_order_relaxed);

    const auto unused = ring_->size() - (in - out);
    if (unused < size) {
      return nullptr;
    }

    // The current block has enough free space
    if (ring_->size() - (in & ring_->mask()) >= size) {
      wrapped_ = false;
      return &ring_->data_[in & ring_->mask()];
    }

    // new_pos is in the next block
    if ((out & ring_->mask()) >= size) {
      wrapped_ = true;
      return &ring_->data_[0];
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
  void Commit(const T * /* unused */, const size_t size) {
    if (size == 0) return;  // Discard empty data

    // only written from push thread
    const auto in = ring_->in_.load(std::memory_order_relaxed);

    if (wrapped_) {
      ring_->last_.store(in, std::memory_order_relaxed);
      ring_->in_.store(ring_->next_buffer(in) + size, std::memory_order_release);
    } else {
      // Whenever we wrap around, we update the last variable to ensure logical
      // consistency.
      const auto new_pos = in + size;
      if ((new_pos & ring_->mask()) == 0) {
        ring_->last_.store(new_pos, std::memory_order_relaxed);
      }
      ring_->in_.store(new_pos, std::memory_order_release);
    }

    ring_->prod_notifier_.notify_when_blocking();
  }

 private:
  std::shared_ptr<Mq<T>> ring_;
  bool wrapped_;
};

template <typename T>
class Consumer {
 public:
  explicit Consumer(const std::shared_ptr<Mq<T>> &ring) : ring_(ring) {}

  /**
   * Gets a pointer to the contiguous block in the buffer, and returns the size of that block. automatically retry until
   * timeout
   * @param timeout The maximum waiting time
   * @param other_condition Other wake-up conditions
   * @return pointer to the contiguous block
   */
  template <typename Condition>
  DataPacket<T> ReadOrWait(
      const std::chrono::milliseconds timeout, Condition other_condition = []() { return false; }) {
    DataPacket<T> ptr;
    ring_->prod_notifier_.wait_for(timeout, [&] { return (ptr = Read()).remain() > 0 || other_condition(); });
    return ptr;
  }
  DataPacket<T> ReadOrWait(const std::chrono::milliseconds timeout) {
    return ReadOrWait(timeout, [] { return false; });
  }

  DataPacket<T> Read() {
    const auto in = ring_->in_.load(std::memory_order_acquire);
    const auto last = ring_->last_.load(std::memory_order_relaxed);
    const auto out = ring_->out_.load(std::memory_order_relaxed);

    if (out == in) {
      return DataPacket<T>{out};
    }

    const auto cur_in = in & ring_->mask();
    const auto cur_out = out & ring_->mask();

    // read and write are still in the same block
    if (cur_out < cur_in) {
      return DataPacket<T>{in, Packet<T>(in - out, &ring_->data_[cur_out])};
    }

    // read and write are in different blocks, read the current remaining data
    if (out != last) {
      Packet<T> group0{last - out, &ring_->data_[cur_out]};
      Packet<T> group1{cur_in, cur_in != 0 ? &ring_->data_[0] : nullptr};
      return DataPacket<T>{in, group0, group1};
    }

    if (cur_in == 0) {
      return DataPacket<T>{in};
    }

    return DataPacket<T>{in, Packet<T>{cur_in, &ring_->data_[0]}};
  }

  void Release(const DataPacket<T> &data) {
    ring_->out_.store(data.end_index_, std::memory_order_release);
    ring_->cons_notifier_.notify_when_blocking();
  }

 private:
  std::shared_ptr<Mq<T>> ring_;
};

}  // namespace spsc
}  // namespace ulog
