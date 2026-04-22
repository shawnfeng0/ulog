//
// Epoch-based lightweight notifier (a.k.a. eventcount, Dmitry Vyukov style).
//

#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace ulog {

/**
 * @brief Eventcount: a mostly lock-free notifier built around an epoch counter.
 *
 * Compared to LiteNotifier, which relies on two std::atomic_thread_fence(seq_cst)
 * to close the classic Dekker StoreLoad race between "state" and "waiters_",
 * Eventcount replaces the out-of-band state with an internally owned epoch_
 * counter that is bumped on every notify_all(). The protocol is:
 *
 *   waiter:   e = epoch_.load(acquire)       // observe a version
 *             if (pred()) return;            // re-check after snapshot
 *             register as a waiter;
 *             park in cv.wait until (epoch_ != e || pred())
 *
 *   notifier: (caller updates external state with release semantics)
 *             epoch_.fetch_add(1);           // publish a new version
 *             if (waiters_ > 0) cv.notify_all();
 *
 * Correctness relies on sequential consistency between four atomic operations:
 *   - producer:  A = epoch_.fetch_add(seq_cst)  <  B = waiters_.load(seq_cst)
 *   - waiter:    D = waiters_.fetch_add(seq_cst) <  E = epoch_.load(seq_cst)
 * In the single total order of seq_cst ops, either D < A (so B sees D and the
 * notifier wakes us) or A < D (so E sees the bumped epoch and we skip cv.wait).
 * Either way there is no missed wakeup, without needing standalone fences.
 *
 * External state visibility is carried by epoch_'s release/acquire edge:
 * producers update state before bumping epoch_, waiters load epoch_ with
 * acquire before re-checking pred().
 */
class Eventcount {
 public:
  template <typename Predicate>
  void wait(Predicate pred) {
    while (true) {
      if (pred()) return;
      const uint64_t e = epoch_.load(std::memory_order_acquire);
      if (pred()) return;

      std::unique_lock<std::mutex> lock(mutex_);
      waiters_.fetch_add(1, std::memory_order_seq_cst);
      if (epoch_.load(std::memory_order_seq_cst) == e && !pred()) {
        cv_.wait(lock, [&] {
          return epoch_.load(std::memory_order_acquire) != e || pred();
        });
      }
      waiters_.fetch_sub(1, std::memory_order_relaxed);
      // Loop: an epoch bump may be "spurious" w.r.t. our predicate, so re-check.
    }
  }

  template <typename Rep, typename Period, typename Predicate>
  bool wait_for(const std::chrono::duration<Rep, Period>& timeout, Predicate pred) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (true) {
      if (pred()) return true;
      const uint64_t e = epoch_.load(std::memory_order_acquire);
      if (pred()) return true;

      std::unique_lock<std::mutex> lock(mutex_);
      waiters_.fetch_add(1, std::memory_order_seq_cst);
      bool not_timed_out = true;
      if (epoch_.load(std::memory_order_seq_cst) == e && !pred()) {
        not_timed_out = cv_.wait_until(lock, deadline, [&] {
          return epoch_.load(std::memory_order_acquire) != e || pred();
        });
      }
      waiters_.fetch_sub(1, std::memory_order_relaxed);

      if (pred()) return true;
      if (!not_timed_out) return false;
      // Epoch changed but pred still false: loop with remaining time.
    }
  }

  void notify_all() {
    epoch_.fetch_add(1, std::memory_order_seq_cst);
    if (waiters_.load(std::memory_order_seq_cst) > 0) {
      std::lock_guard<std::mutex> lock(mutex_);
      cv_.notify_all();
    }
  }

 private:
  std::atomic<uint64_t> epoch_{0};
  std::atomic<int> waiters_{0};
  std::mutex mutex_;
  std::condition_variable cv_;
};

}  // namespace ulog
