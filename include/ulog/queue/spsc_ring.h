#pragma once

#include <atomic>
#include <cstddef>
#include <utility>

#include "power_of_two.h"

namespace ulog {
namespace spsc {

template <typename T>
class Mq;

/**
 * Data structure used when reading data externally
 */
template <typename T>
struct Packet {
  explicit Packet(const size_t s = 0, T *d = nullptr) : size(s), data(d) {}
  Packet(const Packet &other) = default;
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

 public:
  explicit DataPacket(const uint32_t end_index, Packet<T> group0 = Packet<T>{}, Packet<T> group1 = Packet<T>{})
      : end_index_(end_index), group0_(std::move(group0)), group1_(std::move(group1)) {}

  explicit operator bool() const noexcept { return remain() > 0; }
  size_t remain() const { return (group0_ ? 1 : 0) + (group1_ ? 1 : 0); }

  Packet<T> next() {
    if (group0_) return std::move(group0_);
    if (group1_) return std::move(group1_);
    return Packet<T>{0, nullptr};
  }

 private:
  const uint32_t end_index_;
  Packet<T> group0_;
  Packet<T> group1_;
};

template <typename T = char>
class Mq : public std::enable_shared_from_this<Mq<T>> {

  struct Private {
    explicit Private() = default;
  };

 public:
  explicit Mq(size_t num_elements, Private) : out_(0), in_(0), last_(0), wrapped_(false) {
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

  /**
   * Try to reserve space of size
   * @param size size of space to reserve
   * @return data pointer if successful, otherwise nullptr
   */
  T *Reserve(const size_t size) {
    const auto out = out_.load(std::memory_order_relaxed);
    const auto in = in_.load(std::memory_order_relaxed);

    const auto unused = this->size() - (in - out);
    if (unused < size) {
      return nullptr;
    }

    // The current block has enough free space
    if (this->size() - (in & mask()) >= size) {
      wrapped_ = false;
      return &data_[in & mask()];
    }

    // new_pos is in the next block
    if ((out & mask()) >= size) {
      wrapped_ = true;
      return &data_[0];
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

  DataPacket<T> TryRead() {
    const auto in = in_.load(std::memory_order_acquire);
    const auto last = last_.load(std::memory_order_relaxed);
    const auto out = out_.load(std::memory_order_relaxed);

    if (out == in) {
      return DataPacket<T>{out};
    }

    const auto cur_in = in & mask();
    const auto cur_out = out & mask();

    // read and write are still in the same block
    if (cur_out < cur_in) {
      return DataPacket<T>{in, Packet<T>(in - out, &data_[cur_out])};
    }

    // read and write are in different blocks, read the current remaining data
    if (out != last) {
      Packet<T> group0{last - out, &data_[cur_out]};
      Packet<T> group1{cur_in, cur_in != 0 ? &data_[0] : nullptr};
      return DataPacket<T>{in, group0, group1};
    }

    if (cur_in == 0) {
      return DataPacket<T>{in};
    }

    return DataPacket<T>{in, Packet<T>{cur_in, &data_[0]}};
  }

  void Release(const DataPacket<T> &data) { out_.store(data.end_index_, std::memory_order_release); }

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
  bool wrapped_;
};

}  // namespace spsc
}  // namespace ulog
