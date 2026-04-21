// 对照组：模拟修复前的 MPMC —— 生产者直接用 cons_head 判空间，没有 cons_tail
#include "../relacy/relacy/relacy_std.hpp"
#include <cstdint>

static constexpr uint32_t N = 2;

struct Slot {
    std::atomic<int> state;
    VAR_T(uint32_t) payload;
};

struct BuggyRing {
    Slot slots[N];
    std::atomic<uint32_t> prod_head;
    std::atomic<uint32_t> cons_head;

    void init() {
        for (uint32_t i = 0; i < N; ++i) {
            slots[i].state($).store(0, std::memory_order_relaxed);
            VAR(slots[i].payload) = 0;
        }
        prod_head($).store(0, std::memory_order_relaxed);
        cons_head($).store(0, std::memory_order_relaxed);
    }

    bool produce(uint32_t value) {
        for (int spin = 0; spin < 50; ++spin) {
            uint32_t head = prod_head($).load(std::memory_order_relaxed);
            // BUG: 用 cons_head（claim 指针）判空间；consumer CAS 成功即推进，
            //      但 memset/state=0 还没做完 —— 生产者抢跑
            uint32_t tail = cons_head($).load(std::memory_order_acquire);
            if (head - tail >= N) { rl::yield(1, $); continue; }
            uint32_t next = head + 1;
            if (!prod_head($).compare_exchange_weak(head, next, std::memory_order_relaxed)) continue;
            uint32_t idx = head % N;
            VAR(slots[idx].payload) = value;                             // 与 consumer 清零 payload race!
            slots[idx].state($).store(2, std::memory_order_release);
            return true;
        }
        return false;
    }

    bool consume(uint32_t& out) {
        for (int spin = 0; spin < 50; ++spin) {
            uint32_t head = cons_head($).load(std::memory_order_relaxed);
            uint32_t prod = prod_head($).load(std::memory_order_acquire);
            if (head == prod) { rl::yield(1, $); continue; }
            uint32_t next = head + 1;
            if (!cons_head($).compare_exchange_weak(head, next, std::memory_order_relaxed)) continue;
            uint32_t idx = head % N;
            while (slots[idx].state($).load(std::memory_order_acquire) != 2) rl::yield(1, $);
            out = VAR(slots[idx].payload);
            VAR(slots[idx].payload) = 0;                                  // 清零（BUG: 可能与下一 producer 的写 race）
            slots[idx].state($).store(0, std::memory_order_relaxed);
            return true;
        }
        return false;
    }
};

struct mpmc_buggy_test : rl::test_suite<mpmc_buggy_test, 4> {
    BuggyRing ring;
    void before() { ring.init(); }
    void thread(unsigned tid) {
        if (tid < 2) { ring.produce((tid + 1) * 100 + 7); }
        else         { uint32_t got = 0; if (ring.consume(got)) { RL_ASSERT(got == 107 || got == 207); } }
    }
};

int main() {
    rl::test_params p; p.iteration_count = 20000;
    rl::simulate<mpmc_buggy_test>(p);
    return 0;
}
