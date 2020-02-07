//
// Created by fs on 2/5/20.
//

#ifndef ULOG_INCLUDE_ULOG_FIFOPOWEROF2_H_
#define ULOG_INCLUDE_ULOG_FIFOPOWEROF2_H_

#include <pthread.h>
#include <stdint.h>
#include <string.h>

class FifoPowerOfTwo {
#define _is_power_of_2(x) ((x) != 0 && (((x) & ((x)-1)) == 0))
public:
  FifoPowerOfTwo(void *buffer, unsigned int buf_size,
                 unsigned int element_size = 1)
      : data_((unsigned char *)buffer), element_size_(element_size), in_(0),
        out_(0), need_delete_(false) {
    if (element_size == 0) {
      mask_ = 0;
      data_ = nullptr;
      return;
    }
    unsigned int num_elements = buf_size / element_size;

    // round down to the next power of 2, since our 'let the indices wrap'
    // technique works only in this case.
    // The kernel is roundup_pow_of_two, i think it is wrong.
    num_elements = RounddownPowOfTwo(num_elements);

    if (num_elements < 2) {
      mask_ = 0;
      data_ = nullptr;
      return;
    }
    mask_ = num_elements - 1;
  }

  explicit FifoPowerOfTwo(unsigned int num_elements,
                          unsigned int element_size = 1)
      : element_size_(element_size), in_(0), out_(0), need_delete_(true) {
    // round up to the next power of 2, since our 'let the indices wrap'
    // technique works only in this case.
    num_elements = RoundupPowOfTwo(num_elements);

    if (num_elements < 2 || element_size == 0) {
      mask_ = 0;
      data_ = nullptr;
      return;
    }

    data_ = new unsigned char[num_elements * element_size];

    if (!data_) {
      mask_ = 0;
      return;
    }
    mask_ = num_elements - 1;
  }

  ~FifoPowerOfTwo() {
    if (need_delete_ && data_)
      delete data_;
  }

  // A packet is entered, either completely written or discarded.
  unsigned int InPacket(const void *buf, unsigned int num_elements) {
    LockGuard lg(mutex_);
    unsigned int unused = Unused();

    if (unused < num_elements) {
      num_dropped_ += num_elements;
      peak_ = mask_ + 1;
      return 0;
    }

    peak_ = Max(peak_, mask_ + 1 - unused);

    CopyInLocked(buf, num_elements, in_);
    in_ += num_elements;
    return num_elements;
  }

  unsigned int In(const void *buf, unsigned int num_elements) {
    LockGuard lg(mutex_);
    unsigned int unused = Unused();
    peak_ = Max(peak_, mask_ + 1 - unused);
    num_elements = Min(num_elements, unused);

    CopyInLocked(buf, num_elements, in_);
    in_ += num_elements;
    num_dropped_ += unused - num_elements;
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
    LockGuard lg(mutex_);
    out_ = in_;
  }

  bool IsFull() { return Used() - mask_; }
  bool IsEmpty() { return in_ == out_; }

  /**
   * Check if the fifo is initialized
   * Return %true if fifo is initialized, otherwise %false.
   * Assumes the fifo was 0 before.
   */
  bool Initialized() const { return mask_ != 0; };
  unsigned int GetNumDropped() const { return num_dropped_; }
  unsigned int GetPeak() const { return peak_; }
  unsigned int GetSize() const { return mask_ + 1; }

  unsigned int OutPeek(void *buf, unsigned int num_elements) {
    LockGuard lg(mutex_);
    num_elements = Min(num_elements, Used());
    CopyOutLocked(buf, num_elements, out_);
    return num_elements;
  }

  unsigned int Out(void *buf, unsigned int num_elements) {
    LockGuard lg(mutex_);
    num_elements = Min(num_elements, Used());
    CopyOutLocked(buf, num_elements, out_);
    out_ += num_elements;
    return num_elements;
  }

private:
  unsigned int in_{};   // data is added at offset (in % size)
  unsigned int out_{};  // data is extracted from off. (out % size)
  unsigned char *data_; // the buffer holding the data
  bool need_delete_;    // Need to release data memory space according to the
                        // initialization method
  unsigned int
      mask_; // (Constant) Mask used to match the correct in / out pointer
  const unsigned int element_size_; // the size of the element
  unsigned int num_dropped_{};      // Number of dropped elements
  unsigned int peak_{};             // fifo peak
  class Mutex {
  public:
    Mutex() { pthread_mutex_init(&mutex_, nullptr); }
    ~Mutex() { pthread_mutex_destroy(&mutex_); }
    void Lock() { pthread_mutex_lock(&mutex_); }
    void Unlock() { pthread_mutex_unlock(&mutex_); }

  private:
    pthread_mutex_t mutex_{};
  } mutex_; // TODO: If there is only one input and one output thread, then
            // locks are not necessary

  class LockGuard {
  public:
    explicit LockGuard(Mutex &m) : m_(m) { m.Lock(); }
    ~LockGuard() { m_.Unlock(); }

  private:
    Mutex &m_;
  };

  void CopyInLocked(const void *src, unsigned int len, unsigned int off) const {
    unsigned int size = mask_ + 1;

    off &= mask_;
    if (element_size_ != 1) {
      off *= element_size_;
      size *= element_size_;
      len *= element_size_;
    }
    unsigned int l = Min(len, size - off);

    memcpy(data_ + off, src, l);
    memcpy(data_, (unsigned char *)src + l, len - l);
  }

  void CopyOutLocked(void *dst, unsigned int len, unsigned int off) const {
    unsigned int size = mask_ + 1;
    unsigned int l;

    off &= mask_;
    if (element_size_ != 1) {
      off *= element_size_;
      size *= element_size_;
      len *= element_size_;
    }
    l = Min(len, size - off);

    memcpy(dst, data_ + off, l);
    memcpy((unsigned char *)dst + l, data_, len - l);
  }

  unsigned int Unused() const {
    unsigned int unused = mask_ + 1 - (in_ - out_);
    return unused;
  }
  unsigned int Used() const { return in_ - out_; }

  static inline unsigned int Min(unsigned int x, unsigned int y) {
    return x < y ? x : y;
  }
  static inline unsigned int Max(unsigned int x, unsigned int y) {
    return x > y ? x : y;
  }

  // round up to nearest power of two
  static inline unsigned long RoundupPowOfTwo(unsigned long n) {
    return 1UL << (unsigned int)fls(n - 1U);
  }

  // round down to nearest power of two
  static inline unsigned long RounddownPowOfTwo(unsigned long n) {
    return 1UL << (fls(n) - 1U);
  }

  // fls: find last bit set.
  static inline int fls(uint64_t x) { return 64 - clz(x); }
  static inline int fls(uint32_t x) { return 32 - clz(x); }

  // Returns the number of leading 0-bits in x, starting at the most significant
  // bit position. If x is 0, the result is undefined.
  static inline int clz(uint32_t x) { return clz((uint64_t)x) - 32; }
  static inline int clz(uint64_t x) {
    int r = 0;
    if (!(x & 0xFFFFFFFF00000000))
      r += 32, x <<= 32U;
    if (!(x & 0xFFFF000000000000))
      r += 16, x <<= 16U;
    if (!(x & 0xFF00000000000000))
      r += 8, x <<= 8U;
    if (!(x & 0xF000000000000000))
      r += 4, x <<= 4U;
    if (!(x & 0xC000000000000000))
      r += 2, x <<= 2U;
    if (!(x & 0x8000000000000000))
      r += 1;
    return r;
  }
};

#endif // ULOG_INCLUDE_ULOG_FIFOPOWEROF2_H_
