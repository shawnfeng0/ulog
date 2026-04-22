//
// Created by shawn on 24-11-17.
//

#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>

namespace ulog {

/**
 * @brief In conjunction with a lock-free queue, this notifier should be used when the queue is full or empty, as
 * retries can affect performance.
 */
class LiteNotifier {
 public:
  template <typename Predicate>
  void wait(Predicate pred) {
    WaitImpl(pred, [&](std::unique_lock<std::mutex>& lk) { cv_.wait(lk, pred); });
  }

  template <typename Rep, typename Period, typename Predicate>
  bool wait_for(const std::chrono::duration<Rep, Period>& timeout, Predicate pred) {
    bool ok = true;
    WaitImpl(pred, [&](std::unique_lock<std::mutex>& lk) { ok = cv_.wait_for(lk, timeout, pred); });
    return ok;
  }

  void notify_all() {
    // Pair with the seq_cst fence in WaitImpl(): ensures the caller's state
    // store happens-before this waiters_ load, avoiding missed wakeup
    // (Dekker-style StoreLoad race between state/waiters_).
    std::atomic_thread_fence(std::memory_order_seq_cst);
    if (waiters_.load(std::memory_order_relaxed) > 0) {
      std::lock_guard<std::mutex> lock(mutex_);
      cv_.notify_all();
    }
  }

 private:
  template <typename Predicate, typename Waiter>
  void WaitImpl(Predicate pred, Waiter waiter) {
    if (pred()) return;
    std::unique_lock<std::mutex> lock(mutex_);
    waiters_.fetch_add(1, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    waiter(lock);
    waiters_.fetch_sub(1, std::memory_order_relaxed);
  }

  std::atomic<int> waiters_{0};
  std::mutex mutex_;
  std::condition_variable cv_;
};

}  // namespace ulog