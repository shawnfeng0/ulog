// Relacy 模型：LiteNotifier 修复前版本 —— missed wakeup / 死锁
//
// 对应源码（修复前）：
//   waiters_.fetch_add(1, release)   // 在 wait 中
//   waiters_.load(acquire)           // 在 notify_* 中
//
// 与调用者（producer 用 release 发布 state，consumer 用 acquire 读 state）组合后，
// producer "state.store -> waiters_.load" 和 consumer "waiters_.fetch_add -> state.load"
// 两对跨变量访问缺少 StoreLoad 屏障（seq_cst），典型 Dekker 问题，可丢唤醒。
//
// 期望结果：Relacy 报 DEADLOCK
#include "../relacy/relacy/relacy_std.hpp"

struct LiteNotifierBuggy {
    std::atomic<int> waiters_;
    std::mutex mutex_;
    std::condition_variable cv_;

    LiteNotifierBuggy() { waiters_($).store(0, std::memory_order_relaxed); }

    template <typename Pred>
    void wait(Pred pred) {
        if (pred()) return;
        mutex_.lock($);
        waiters_($).fetch_add(1, std::memory_order_release);
        while (!pred()) cv_.wait(mutex_, $);
        waiters_($).fetch_sub(1, std::memory_order_release);
        mutex_.unlock($);
    }

    void notify_all() {
        if (waiters_($).load(std::memory_order_acquire) > 0) {
            mutex_.lock($);
            cv_.notify_all($);
            mutex_.unlock($);
        }
    }
};

struct notifier_test : rl::test_suite<notifier_test, 2> {
    std::atomic<int> state;
    LiteNotifierBuggy notif;
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
