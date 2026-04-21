// Relacy 模型：复刻 ulog mpsc_ring.h 的同步协议（简化版）
// - 多生产者 / 单消费者，定长 N 槽
// - 每槽：std::atomic<int> state (0=free, 2=ready) + 非原子 payload
// - prod_head_：生产者共享，CAS 推进
// - cons_head_：单消费者专享，release-store 推进；生产者用 acquire-load 判空间
// 与 MPMC 的关键区别：消费者只有一个 -> 无需拆分 cons_head/cons_tail
// 生产者：load cons_head_(acquire) 判空间 -> CAS prod_head_ -> 写 payload -> state=ready(release)
// 消费者：load state(acquire) 判就绪 -> 读 payload -> 清零槽位 -> store cons_head_(release)
#include "../relacy/relacy/relacy_std.hpp"
#include <cstdint>

static constexpr uint32_t N = 2;

struct Slot {
    std::atomic<int> state;
    VAR_T(uint32_t) payload;
};

struct MpscRing {
    Slot slots[N];
    std::atomic<uint32_t> prod_head_;
    std::atomic<uint32_t> cons_head_;

    void init() {
        for (uint32_t i = 0; i < N; ++i) {
            slots[i].state($).store(0, std::memory_order_relaxed);
            VAR(slots[i].payload) = 0;
        }
        prod_head_($).store(0, std::memory_order_relaxed);
        cons_head_($).store(0, std::memory_order_relaxed);
    }

    bool produce(uint32_t value) {
        for (int spin = 0; spin < 50; ++spin) {
            uint32_t head = prod_head_($).load(std::memory_order_relaxed);
            uint32_t cons = cons_head_($).load(std::memory_order_acquire);
            if (head - cons >= N) { rl::yield(1, $); continue; }
            uint32_t next = head + 1;
            if (!prod_head_($).compare_exchange_weak(head, next, std::memory_order_relaxed)) continue;
            uint32_t idx = head % N;
            VAR(slots[idx].payload) = value;
            slots[idx].state($).store(2, std::memory_order_release);
            return true;
        }
        return false;
    }

    bool consume(uint32_t& out) {
        uint32_t cons = cons_head_($).load(std::memory_order_relaxed);
        uint32_t prod = prod_head_($).load(std::memory_order_acquire);
        if (cons == prod) return false;
        uint32_t idx = cons % N;
        if (slots[idx].state($).load(std::memory_order_acquire) != 2) return false;
        out = VAR(slots[idx].payload);
        VAR(slots[idx].payload) = 0;
        slots[idx].state($).store(0, std::memory_order_relaxed);
        cons_head_($).store(cons + 1, std::memory_order_release);
        return true;
    }
};

struct mpsc_test : rl::test_suite<mpsc_test, 3> {
    MpscRing ring;
    std::atomic<int> consumed_count;

    void before() {
        ring.init();
        consumed_count($).store(0, std::memory_order_relaxed);
    }

    void thread(unsigned tid) {
        if (tid < 2) {
            uint32_t v = (tid + 1) * 100 + 7;
            while (!ring.produce(v)) rl::yield(1, $);
        } else {
            int got = 0;
            for (int spin = 0; spin < 200 && got < 2; ++spin) {
                uint32_t v = 0;
                if (ring.consume(v)) {
                    RL_ASSERT(v == 107 || v == 207);
                    ++got;
                } else {
                    rl::yield(1, $);
                }
            }
            consumed_count($).store(got, std::memory_order_relaxed);
        }
    }

    void after() {
        RL_ASSERT(consumed_count($).load(std::memory_order_relaxed) == 2);
    }
};

int main() {
    rl::test_params p;
    p.iteration_count = 10000;
    rl::simulate<mpsc_test>(p);
    return 0;
}
