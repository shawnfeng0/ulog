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
  void wait(Predicate p) {
    if (p()) return;

    std::unique_lock<std::mutex> lk(mtx_);
    waiter_count_.fetch_add(1, std::memory_order_relaxed);
    cv_.wait(lk, [&] { return p(); });
    waiter_count_.fetch_sub(1, std::memory_order_relaxed);
  }

  template <typename Predicate>
  bool wait_for(std::chrono::milliseconds timeout, Predicate p) {
    if (p()) return true;

    std::unique_lock<std::mutex> lk(mtx_);
    waiter_count_.fetch_add(1, std::memory_order_relaxed);
    auto ret = cv_.wait_for(lk, timeout, [&] { return p(); });
    waiter_count_.fetch_sub(1, std::memory_order_relaxed);
    return ret;
  }

  void notify_when_blocking() {
    if (waiter_count_.load(std::memory_order_relaxed) > 0) {
      std::unique_lock<std::mutex> lk(mtx_);
      cv_.notify_all();
    }
  }

 private:
  std::mutex mtx_;
  std::condition_variable cv_;
  std::atomic_int32_t waiter_count_{false};
};

}  // namespace ulog