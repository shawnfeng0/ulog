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
  LiteNotifier() : waiters_(0) {}

  template <typename Predicate>
  void wait(Predicate pred) {
    if (pred()) return;

    std::unique_lock<std::mutex> lock(mutex_);
    waiters_.fetch_add(1, std::memory_order_release);
    cv_.wait(lock, pred);
    waiters_.fetch_sub(1, std::memory_order_release);
  }

  template <typename Rep, typename Period, typename Predicate>
  bool wait_for(const std::chrono::duration<Rep, Period>& timeout, Predicate pred) {
    if (pred()) return true;

    std::unique_lock<std::mutex> lock(mutex_);
    waiters_.fetch_add(1, std::memory_order_release);
    const bool result = cv_.wait_for(lock, timeout, pred);
    waiters_.fetch_sub(1, std::memory_order_release);
    return result;
  }

  void notify_all() {
    if (waiters_.load(std::memory_order_acquire) > 0) {
      std::lock_guard<std::mutex> lock(mutex_);
      cv_.notify_all();
    }
  }

  void notify_one() {
    if (waiters_.load(std::memory_order_acquire) > 0) {
      std::lock_guard<std::mutex> lock(mutex_);
      cv_.notify_one();
    }
  }

 private:
  std::atomic<int> waiters_;
  std::mutex mutex_;
  std::condition_variable cv_;
};

}  // namespace ulog