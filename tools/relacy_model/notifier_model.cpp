// Relacy 模型：LiteNotifier 修复版 —— atomic_thread_fence(seq_cst) 方案
//
// 对应源码（修复后）：
//   wait:      waiters_.fetch_add(1, relaxed);
//              std::atomic_thread_fence(seq_cst);   // ★
//              cv_.wait(lock, pred);
//   notify_*:  std::atomic_thread_fence(seq_cst);   // ★
//              if (waiters_.load(relaxed) > 0) { lock; notify; }
//
// fence 建立全局 StoreLoad 序，确保：
// - consumer 的 fetch_add 相对于后续 pred() 中的 state load
// - producer 的 state.store 相对于后续 waiters_.load
// 至少有一侧能看到对方的 "新" 值，从而不会同时错过。
//
// 期望结果：100000 iterations 全通过
#include "../relacy/relacy/relacy_std.hpp"

struct LiteNotifierFixed {
    std::atomic<int> waiters_;
    std::mutex mutex_;
    std::condition_variable cv_;

    LiteNotifierFixed() { waiters_($).store(0, std::memory_order_relaxed); }

    template <typename Pred>
    void wait(Pred pred) {
        if (pred()) return;
        mutex_.lock($);
        waiters_($).fetch_add(1, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_seq_cst, $);
        while (!pred()) cv_.wait(mutex_, $);
        waiters_($).fetch_sub(1, std::memory_order_relaxed);
        mutex_.unlock($);
    }

    void notify_all() {
        std::atomic_thread_fence(std::memory_order_seq_cst, $);
        if (waiters_($).load(std::memory_order_relaxed) > 0) {
            mutex_.lock($);
            cv_.notify_all($);
            mutex_.unlock($);
        }
    }
};

struct notifier_test : rl::test_suite<notifier_test, 2> {
    std::atomic<int> state;
    LiteNotifierFixed notif;
    std::atomic<int> got;

    void before() {
        state($).store(0, std::memory_order_relaxed);
        got($).store(-1, std::memory_order_relaxed);
    }

    void thread(unsigned tid) {
        if (tid == 0) {
            state($).store(1, std::memory_order_release);
            notif.notify_all();
        } else {
            auto pred = [&]() {
                return state($).load(std::memory_order_acquire) != 0;
            };
            notif.wait(pred);
            got($).store(state($).load(std::memory_order_acquire),
                         std::memory_order_relaxed);
        }
    }

    void after() {
        RL_ASSERT(got($).load(std::memory_order_relaxed) == 1);
    }
};

int main() {
    rl::test_params p;
    p.iteration_count = 100000;
    rl::simulate<notifier_test>(p);
    return 0;
}
