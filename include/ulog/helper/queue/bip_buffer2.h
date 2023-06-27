#pragma once

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <vector>

#include "power_of_two.h"

#define BIPBUFFER2_DEBUG 0

namespace ulog {
template <typename T = char>
class BipBuffer2 {
 public:
  explicit BipBuffer2(size_t num_elements)
      : read_index_(0), write_index_(0), last_index_(0) {
    if (num_elements < 2)
      num_elements = 2;
    else {
      // round up to the next power of 2, since our 'let the indices wrap'
      // technique works only in this case.
      num_elements = queue::RoundUpPowOfTwo(num_elements);
    }
    buffer_.resize(num_elements);
  }

  ~BipBuffer2() = default;

  /**
   * Try to reserve space of size size
   * @param size size of space to reserve
   * @return data pointer if successful, otherwise nullptr
   */
  T* Reserve(size_t size) {
    const auto read = read_index_.load(std::memory_order_relaxed);
    const auto write = write_index_.load(std::memory_order_relaxed);

#if BIPBUFFER2_DEBUG == 1
    printf("Reserve: read: %u, write: %u, size: %u\r\n", read, write, size);
#endif

    const auto cur_write = write & mask();
    const auto cur_read = read & mask();

    if (cur_write < cur_read) {
      const auto writable = cur_read - cur_write;
      return (writable > size) ? &buffer_[cur_write] : nullptr;

    } else {  // cur_write >= cur_read
      // read and write are inside the same block
      const auto tail_writable = this->size() - cur_write;
      const auto head_writable = cur_read;

      if (tail_writable > size) {
        return &buffer_[cur_write];

      } else if (head_writable > size) {
        last_index_.store(write, std::memory_order_relaxed);
        write_index_.store(next_buffer(write), std::memory_order_release);
        return &buffer_[0];

      } else {
        return nullptr;
      }
    }
  }

  /**
   * Commits the data to the buffer, so that it can be read out.
   * @param size the size of the data to be committed
   *
   * NOTE:
   * The validity of the size is not checked, it needs to be within the range
   * returned by the Reserve function.
   */
  void Commit(size_t size) {
    // only written from push thread
    const auto write = write_index_.load(std::memory_order_relaxed);

#if BIPBUFFER2_DEBUG == 1
    printf("Commit: write: %u, size: %u\r\n", write, size);
#endif

    // Whenever we wrap around, we update the last variable to ensure logical
    // consistency.
    if (((write + size) & mask()) == 0) {
      last_index_.store(write + size, std::memory_order_relaxed);
    }
    write_index_.store(write + size, std::memory_order_release);
  }

  /**
   * Gets a pointer to the contiguous block in the buffer, and returns the size
   * of that block.
   * @param size returns the size of the contiguous block
   * @return pointer to the contiguous block
   */
  T* Read(size_t* size) {
    const auto write = write_index_.load(std::memory_order_acquire);
    const auto last = last_index_.load(std::memory_order_relaxed);
    auto read = read_index_.load(std::memory_order_relaxed);

#if BIPBUFFER2_DEBUG == 1
    printf("Read: read: %u, write: %u, last: %u\r\n", read, write, last);
#endif

    if (read == write) {
      *size = 0;
      return nullptr;
    }

    // The current block has been read, "write" has reached the next block
    if (read == last) {
      const auto write_block_pos = write & ~mask();
      const auto read_block_pos = read & ~mask();
      if (read_block_pos != write_block_pos) {
        read = write_block_pos;
        read_index_.store(write_block_pos, std::memory_order_relaxed);
      }
    }

    // If last is from the previous round, a maximum value should be
    // calculated, in fact it should be a negative value. We only care about
    // numbers larger than read_index.
    const auto distance_from_last = last - read;
    const auto distance_from_write = write - read;
    const auto readable_size =
        distance_from_last < distance_from_write && distance_from_last != 0
            ? distance_from_last
            : distance_from_write;

    if (readable_size > 0) {
      *size = readable_size;
      return &buffer_[read & mask()];
    }

    *size = 0;
    return nullptr;
  }

  /**
   * Releases data from the buffer, so that more data can be written in.
   * @param size the size of the data to be released
   *
   * Note:
   *
   * The validity of the size is not checked, it needs to be within the range
   * returned by the Read function.
   *
   */
  void Release(size_t size) {
    auto read = read_index_.load(std::memory_order_relaxed);

#if BIPBUFFER2_DEBUG == 1
    printf("Release: read: %u, size: %u\r\n", read, size);
#endif

    read_index_.store(read + size, std::memory_order_release);
  }

 private:
  size_t size() const { return buffer_.size(); }
  size_t mask() const { return size() - 1; }
  size_t inline next_buffer(size_t index) const {
    return (index & ~mask()) + size();
  }

  std::vector<T> buffer_;

  // for reader thread
  std::atomic<size_t> read_index_;

  // for writer thread
  std::atomic<size_t> write_index_;
  std::atomic<size_t> last_index_;
};
}  // namespace ulog
