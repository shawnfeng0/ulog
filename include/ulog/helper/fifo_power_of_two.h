//
// Created by fs on 2/5/20.
//

#ifndef ULOG_INCLUDE_ULOG_FIFOPOWEROF2_H_
#define ULOG_INCLUDE_ULOG_FIFOPOWEROF2_H_

#include <pthread.h>

#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace ulog {

// Reference from kfifo of linux
class FifoPowerOfTwo {
#define _is_power_of_2(x) ((x) != 0 && (((x) & ((x)-1)) == 0))
 public:
  FifoPowerOfTwo(void *buffer, size_t buf_size, size_t element_size = 1)
      : data_((unsigned char *)buffer),
        element_size_(element_size != 0 ? element_size : 1),
        is_allocated_memory_(false) {
    size_t num_elements = buf_size / element_size_;

    if (num_elements < 2) {
      mask_ = 0;
      data_ = nullptr;
      return;
    }

    // round down to the next power of 2, since our 'let the indices wrap'
    // technique works only in this case.
    // The kernel is roundup_pow_of_two, maybe it's wrong.
    mask_ = RoundDownPowOfTwo(num_elements) - 1;
  }

  explicit FifoPowerOfTwo(size_t num_elements, size_t element_size = 1)
      : element_size_(element_size != 0 ? element_size : 1),
        is_allocated_memory_(true) {
    if (num_elements < 2) num_elements = 2;

    // round up to the next power of 2, since our 'let the indices wrap'
    // technique works only in this case.
    num_elements = RoundUpPowOfTwo(num_elements);

    data_ = new unsigned char[num_elements * element_size_];

    if (!data_) {
      mask_ = 0;
      return;
    }
    mask_ = num_elements - 1;
  }
  FifoPowerOfTwo(const FifoPowerOfTwo &) = delete;
  FifoPowerOfTwo &operator=(const FifoPowerOfTwo &) = delete;

  ~FifoPowerOfTwo() {
    if (is_allocated_memory_ && data_) delete[] data_;
  }

  // A packet is entered, either completely written or discarded.
  size_t InPacket(const void *buf, size_t num_elements) {
    if (!buf) {
      return 0;
    }

    std::lock_guard<std::mutex> lg(mutex_);

    if (unused() < num_elements) {
      num_dropped_ += num_elements;
      peak_ = size();
      return 0;
    }

    peak_ = max(peak_, used());

    CopyInLocked(buf, num_elements, in_);
    in_ += num_elements;

    cond_.notify_all();
    return num_elements;
  }

  size_t In(const void *buf, size_t num_elements) {
    if (!buf) {
      return 0;
    }

    std::lock_guard<std::mutex> lg(mutex_);

    peak_ = max(peak_, used());

    const size_t len = min(num_elements, unused());

    CopyInLocked(buf, len, in_);
    in_ += len;
    num_dropped_ += num_elements - len;

    cond_.notify_all();
    return len;
  }

  size_t OutPeek(void *out_buf, size_t num_elements) {
    if (!out_buf) {
      return 0;
    }

    std::lock_guard<std::mutex> lg(mutex_);
    num_elements = min(num_elements, used());
    CopyOutLocked(out_buf, num_elements, out_);
    return num_elements;
  }

  size_t OutWaitIfEmpty(void *out_buf, size_t num_elements,
                        int32_t time_ms = -1) {
    if (!out_buf) {
      return 0;
    }

    std::unique_lock<std::mutex> lg(mutex_);

    if (-1 == time_ms) {
      while (empty()) cond_.wait(lg);
    } else if (empty()) {
      cond_.wait_for(lg, std::chrono::milliseconds{time_ms});
      if (empty()) return 0;
    }

    num_elements = min(num_elements, used());
    CopyOutLocked(out_buf, num_elements, out_);
    out_ += num_elements;
    return num_elements;
  }

  size_t Out(void *out_buf, size_t num_elements) {
    if (!out_buf) {
      return 0;
    }

    std::lock_guard<std::mutex> lg(mutex_);
    num_elements = min(num_elements, used());
    CopyOutLocked(out_buf, num_elements, out_);
    out_ += num_elements;
    return num_elements;
  }

  /**
   * removes the entire fifo content
   *
   * Note: usage of Reset() is dangerous. It should be only called when the
   * fifo is exclusived locked or when it is secured that no other thread is
   * accessing the fifo.
   */
  void Reset() {
    std::lock_guard<std::mutex> lg(mutex_);
    out_ = in_;
  }

  /**
   * Check if the fifo is initialized
   * Return %true if fifo is initialized, otherwise %false.
   * Assumes the fifo was 0 before.
   */
  bool initialized() const { return mask_ != 0; };

  /* returns the size of the fifo in elements */
  size_t size() const { return mask_ + 1; }

  /* returns the number of used elements in the fifo */
  size_t used() const { return in_ - out_; }
  bool empty() const { return used() == 0; }

  /* returns the number of unused elements in the fifo */
  size_t unused() const { return size() - used(); }

  /* DEBUG: Used to count the number of new data discarded during use */
  size_t num_dropped() const { return num_dropped_; }

  /* DEBUG: Used to count the maximum peak value during use */
  size_t peak() const { return peak_; }

 private:
  size_t in_{};            // data is added at offset (in % size)
  size_t out_{};           // data is extracted from off. (out % size)
  unsigned char *data_{};  // the buffer holding the data
  const bool is_allocated_memory_{
      false};      // Used to identify whether the internal
                   // buffer is allocated internally
  size_t mask_{};  // (Constant) Mask used to match the correct in / out pointer
  const size_t element_size_;  // the size of the element
  size_t num_dropped_{};       // Number of dropped elements
  size_t peak_{};              // fifo peak

  std::mutex mutex_{};
  std::condition_variable cond_{};

  void CopyInLocked(const void *src, size_t len, size_t off) const {
    size_t size = this->size();

    off &= mask_;
    if (element_size_ != 1) {
      off *= element_size_;
      size *= element_size_;
      len *= element_size_;
    }
    size_t l = min(len, size - off);

    memcpy(data_ + off, src, l);
    memcpy(data_, (unsigned char *)src + l, len - l);
  }

  void CopyOutLocked(void *dst, size_t len, size_t off) const {
    size_t size = this->size();
    size_t l;

    off &= mask_;
    if (element_size_ != 1) {
      off *= element_size_;
      size *= element_size_;
      len *= element_size_;
    }
    l = min(len, size - off);

    memcpy(dst, data_ + off, l);
    memcpy((unsigned char *)dst + l, data_, len - l);
  }

  template <typename T>
  static inline T min(T x, T y) {
    return x < y ? x : y;
  }
  template <typename T>
  static inline T max(T x, T y) {
    return x > y ? x : y;
  }

  template <typename T>
  static inline auto RoundPowOfTwo(T n) -> decltype(n) {
    uint64_t value = n;

    // Fill 1
    value |= value >> 1U;
    value |= value >> 2U;
    value |= value >> 4U;
    value |= value >> 8U;
    value |= value >> 16U;
    value |= value >> 32U;

    // Unable to round-up, take the value of round-down
    if (decltype(n)(value + 1) == 0) {
      value >>= 1U;
    }

    return value + 1;
  }

  static inline auto RoundUpPowOfTwo(uint32_t n) -> decltype(n) {
    if (n == 0) return 1;

    // Avoid is already a power of 2
    return RoundPowOfTwo<decltype(n)>(n - 1);
  }

  static inline auto RoundDownPowOfTwo(uint32_t n) -> decltype(n) {
    return RoundPowOfTwo<decltype(n)>(n >> 1U);
  }
};

}  // namespace ulog

#endif  // ULOG_INCLUDE_ULOG_FIFOPOWEROF2_H_
