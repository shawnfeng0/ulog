//
// Created by fs on 2/5/20.
//

#ifndef ULOG_INCLUDE_ULOG_FIFOPOWEROF2_H_
#define ULOG_INCLUDE_ULOG_FIFOPOWEROF2_H_

#include <pthread.h>

#include <cstdint>
#include <cstring>

namespace ulog {

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
    LockGuard lg(mutex_);

    if (available() < num_elements) {
      num_dropped_ += num_elements;
      peak_ = size();
      return 0;
    }

    peak_ = max(peak_, used());

    CopyInLocked(buf, num_elements, in_);
    in_ += num_elements;

    cond_.Sign();
    return num_elements;
  }

  size_t In(const void *buf, size_t num_elements) {
    LockGuard lg(mutex_);

    peak_ = max(peak_, used());

    const size_t len = min(num_elements, available());

    CopyInLocked(buf, len, in_);
    in_ += len;
    num_dropped_ += num_elements - len;

    cond_.Sign();
    return len;
  }

  /**
   * removes the entire fifo content
   *
   * Note: usage of Reset() is dangerous. It should be only called when the
   * fifo is exclusived locked or when it is secured that no other thread is
   * accessing the fifo.
   */
  void Reset() {
    LockGuard lg(mutex_);
    out_ = in_;
  }

  bool full() const { return used() == size(); }
  bool empty() const { return in_ == out_; }

  /**
   * Check if the fifo is initialized
   * Return %true if fifo is initialized, otherwise %false.
   * Assumes the fifo was 0 before.
   */
  bool initialized() const { return mask_ != 0; };
  size_t num_dropped() const { return num_dropped_; }
  size_t peak() const { return peak_; }
  size_t size() const { return mask_ + 1; }

  size_t OutPeek(void *out_buf, size_t num_elements) {
    LockGuard lg(mutex_);
    num_elements = min(num_elements, used());
    CopyOutLocked(out_buf, num_elements, out_);
    return num_elements;
  }

  size_t OutWaitIfEmpty(void *out_buf, size_t num_elements,
                        int32_t time_ms = -1) {
    LockGuard lg(mutex_);

    if (-1 == time_ms) {
      while (empty()) cond_.Wait(mutex_.get());
    } else if (empty()) {
      cond_.WaitFor(mutex_.get(), time_ms);
      if (empty()) return 0;
    }

    num_elements = min(num_elements, used());
    CopyOutLocked(out_buf, num_elements, out_);
    out_ += num_elements;
    return num_elements;
  }

  size_t Out(void *out_buf, size_t num_elements) {
    LockGuard lg(mutex_);
    num_elements = min(num_elements, used());
    CopyOutLocked(out_buf, num_elements, out_);
    out_ += num_elements;
    return num_elements;
  }

 private:
  size_t in_{};                      // data is added at offset (in % size)
  size_t out_{};                     // data is extracted from off. (out % size)
  unsigned char *data_{};            // the buffer holding the data
  bool is_allocated_memory_{false};  // Used to identify whether the internal
                                     // buffer is allocated internally
  size_t mask_{};  // (Constant) Mask used to match the correct in / out pointer
  const size_t element_size_;  // the size of the element
  size_t num_dropped_{};       // Number of dropped elements
  size_t peak_{};              // fifo peak
  class Mutex {
   public:
    Mutex() { pthread_mutex_init(&mutex_, nullptr); }
    ~Mutex() { pthread_mutex_destroy(&mutex_); }
    void Lock() { pthread_mutex_lock(&mutex_); }
    void Unlock() { pthread_mutex_unlock(&mutex_); }
    pthread_mutex_t &get() { return mutex_; }

   private:
    pthread_mutex_t mutex_{};
  } mutex_{};  // TODO: If there is only one input and one output thread, then
               // locks are not necessary

  class Condition {
   public:
    Condition() {
      pthread_condattr_t attr;
      pthread_condattr_init(&attr);
      pthread_condattr_setclock(&attr, clockid_);
      pthread_cond_init(&cond_, &attr);
      pthread_condattr_destroy(&attr);
    }
    ~Condition() { pthread_cond_destroy(&cond_); }
    bool Wait(pthread_mutex_t &mutex) {
      return 0 == pthread_cond_wait(&cond_, &mutex);
    }
    bool WaitFor(pthread_mutex_t &mutex, uint64_t time_ms) {
      struct timespec atime {};
      GenerateFutureTime(clockid_, time_ms, atime);
      return 0 == pthread_cond_timedwait(&cond_, &mutex, &atime);
    }
    void Sign() { pthread_cond_signal(&cond_); }

   private:
    // Increase time_ms time based on the current clockid time
    static inline void GenerateFutureTime(clockid_t clockid, uint64_t time_ms,
                                          struct timespec &out) {
      // Calculate an absolute time in the future
      const decltype(out.tv_nsec) kSec2Nsec = 1000 * 1000 * 1000;
      clock_gettime(clockid, &out);
      uint64_t nano_secs = out.tv_nsec + time_ms * 1000 * 1000;
      out.tv_nsec = nano_secs % kSec2Nsec;
      out.tv_sec += nano_secs / kSec2Nsec;
    }
    pthread_cond_t cond_{};
    static constexpr clockid_t clockid_ = CLOCK_REALTIME;
  } cond_{};

  class LockGuard {
   public:
    explicit LockGuard(Mutex &m) : m_(m) { m.Lock(); }
    ~LockGuard() { m_.Unlock(); }

   private:
    Mutex &m_;
  };

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

  size_t available() const { return size() - used(); }
  size_t used() const { return in_ - out_; }

  template <typename T>
  static inline T min(T x, T y) {
    return x < y ? x : y;
  }
  template <typename T>
  static inline T max(T x, T y) {
    return x > y ? x : y;
  }

  // round up to nearest power of two
  static inline uint64_t RoundUpPowOfTwo(uint64_t n) {
    return (n == 0) ? 1 : (1UL << FindLastSet(n - 1U));
  }

  // round down to nearest power of two
  static inline uint64_t RoundDownPowOfTwo(uint64_t n) {
    return (n == 0) ? 1 : (1UL << (FindLastSet(n) - 1U));
  }

  // Contrary to __builtin_ffs, find the highest 1
  // if x is zero, returns zero.
  static inline unsigned FindLastSet(uint64_t x) {
    return 64 - CountLeadingZero(x);
  }

  // Similar to __builtin_clz:
  // Returns the number of leading 0-bits in x, starting at the most
  // significant bit position.
  static inline unsigned CountLeadingZero(uint64_t x) {
    unsigned r = 0;
    if (!(x & 0xFFFFFFFF00000000)) r += 32, x <<= 32U;
    if (!(x & 0xFFFF000000000000)) r += 16, x <<= 16U;
    if (!(x & 0xFF00000000000000)) r += 8, x <<= 8U;
    if (!(x & 0xF000000000000000)) r += 4, x <<= 4U;
    if (!(x & 0xC000000000000000)) r += 2, x <<= 2U;
    if (!(x & 0x8000000000000000)) r += 1, x <<= 1U;
    if (!(x & 0x8000000000000000)) r += 1;
    return r;
  }
};

}  // namespace ulog

#endif  // ULOG_INCLUDE_ULOG_FIFOPOWEROF2_H_
