// Relacy 模型：Eventcount —— 基于 epoch 计数器的通知器
//
// 协议：
//   wait:    if (pred()) return;
//            e = epoch_.load(acquire);
//            if (pred()) return;
//            lock;
//            waiters_.fetch_add(1, seq_cst);       // D
//            if (epoch_.load(seq_cst) == e         // E
//                && !pred())
//                cv.wait(...);
//            waiters_.fetch_sub(1, relaxed);
//
//   notify:  (caller: state.store(release))
//            epoch_.fetch_add(1, seq_cst);         // A
//            if (waiters_.load(seq_cst) > 0)       // B
//                { lock; notify_all; }
//
// SC 全序中 {A,B,D,E} 必有：
//   - D < A  →  B 看到 waiters_>=1 → 唤醒
//   - A < D  →  E 看到新 epoch    → 不睡
// 任一分支都不会丢失唤醒。
//
// 期望结果：100000 iterations 全通过
#include "../relacy/relacy/relacy_std.hpp"

struct Eventcount {
    std::atomic<unsigned> epoch_;
    std::atomic<int> waiters_;
    std::mutex mutex_;
    std::condition_variable cv_;

    Eventcount() {
        epoch_($).store(0, std::memory_order_relaxed);
        waiters_($).store(0, std::memory_order_relaxed);
    }

    template <typename Pred>
    void wait(Pred pred) {
        while (true) {
            if (pred()) return;
            unsigned e = epoch_($).load(std::memory_order_acquire);
            if (pred()) return;

            mutex_.lock($);
            waiters_($).fetch_add(1, std::memory_order_seq_cst);
            bool need_sleep =
                (epoch_($).load(std::memory_order_seq_cst) == e) && !pred();
            while (need_sleep) {
                cv_.wait(mutex_, $);
                if (epoch_($).load(std::memory_order_acquire) != e || pred())
                    need_sleep = false;
            }
            waiters_($).fetch_sub(1, std::memory_order_relaxed);
            mutex_.unlock($);

            if (pred()) return;
            // otherwise loop
        }
    }

    void notify_all() {
        epoch_($).fetch_add(1, std::memory_order_seq_cst);
        if (waiters_($).load(std::memory_order_seq_cst) > 0) {
            mutex_.lock($);
            cv_.notify_all($);
            mutex_.unlock($);
        }
    }
};

struct eventcount_test : rl::test_suite<eventcount_test, 2> {
    std::atomic<int> state;
    Eventcount notif;
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
    rl::simulate<eventcount_test>(p);
    return 0;
}
