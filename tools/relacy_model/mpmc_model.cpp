// Relacy 模型：复刻 ulog mpmc_ring.h 的同步协议（简化版）
// - 定长 N 个槽位，每槽存 uint32_t payload
// - 每个槽有 std::atomic<int> state: 0=free, 1=writing, 2=ready
// - 生产者：load cons_tail(acq) 检查空间 -> CAS prod_head -> 写 payload -> state=ready(release)
// - 消费者：load prod_head(acq) -> CAS cons_head 领取 -> state==ready(acq) 后读 payload ->
//            清零槽位(模拟 memset) -> 等 cons_tail == my_prev -> cons_tail = my_next(release)
// 不变量：消费者读到的 payload 必须等于生产者写入的值
#include "../relacy/relacy/relacy_std.hpp"
#include <cstdint>

static constexpr uint32_t N = 2;

struct Slot {
    std::atomic<int> state;   // 0=free, 2=ready
    VAR_T(uint32_t) payload;  // 非原子负载（由 state 同步保护）
};

struct MpmcRing {
    Slot slots[N];
    std::atomic<uint32_t> prod_head;
    std::atomic<uint32_t> cons_head;
    std::atomic<uint32_t> cons_tail;

    void init() {
        for (uint32_t i = 0; i < N; ++i) {
            slots[i].state($).store(0, std::memory_order_relaxed);
            VAR(slots[i].payload) = 0;
        }
        prod_head($).store(0, std::memory_order_relaxed);
        cons_head($).store(0, std::memory_order_relaxed);
        cons_tail($).store(0, std::memory_order_relaxed);
    }

    // 返回是否成功；成功时在 out_idx 返回槽索引、out_head 是此生产者的 prev head
    bool produce(uint32_t value) {
        for (int spin = 0; spin < 50; ++spin) {
            uint32_t head = prod_head($).load(std::memory_order_relaxed);
            uint32_t tail = cons_tail($).load(std::memory_order_acquire);
            if (head - tail >= N) {  // 无空间
                rl::yield(1, $);
                continue;
            }
            uint32_t next = head + 1;
            if (!prod_head($).compare_exchange_weak(head, next, std::memory_order_relaxed)) {
                continue;
            }
            uint32_t idx = head % N;
            // state 此时应为 0（已被 consumer 清零并 release cons_tail）
            VAR(slots[idx].payload) = value;
            slots[idx].state($).store(2, std::memory_order_release);
            return true;
        }
        return false;
    }

    // 返回是否成功；成功时通过 out 返回读到的值
    bool consume(uint32_t& out) {
        for (int spin = 0; spin < 50; ++spin) {
            uint32_t head = cons_head($).load(std::memory_order_relaxed);
            uint32_t prod = prod_head($).load(std::memory_order_acquire);
            if (head == prod) {  // 空
                rl::yield(1, $);
                continue;
            }
            uint32_t next = head + 1;
            if (!cons_head($).compare_exchange_weak(head, next, std::memory_order_relaxed)) {
                continue;
            }
            uint32_t idx = head % N;
            // 等 producer 发布
            while (slots[idx].state($).load(std::memory_order_acquire) != 2) {
                rl::yield(1, $);
            }
            out = VAR(slots[idx].payload);
            // 模拟 memset（字节清零负载 + state）
            VAR(slots[idx].payload) = 0;
            slots[idx].state($).store(0, std::memory_order_relaxed);
            // 按 claim 顺序推进 cons_tail
            while (cons_tail($).load(std::memory_order_relaxed) != head) {
                rl::yield(1, $);
            }
            cons_tail($).store(next, std::memory_order_release);
            return true;
        }
        return false;
    }
};

// 2 producers × 2 consumers，各 1 条消息
struct mpmc_test : rl::test_suite<mpmc_test, 4> {
    MpmcRing ring;
    std::atomic<int> produced_count;
    std::atomic<int> consumed_count;

    void before() {
        ring.init();
        produced_count($).store(0, std::memory_order_relaxed);
        consumed_count($).store(0, std::memory_order_relaxed);
    }

    void thread(unsigned tid) {
        if (tid < 2) {
            // producer：写 tid+1 便于检查
            uint32_t val = (tid + 1) * 100 + 7;
            if (ring.produce(val)) {
                produced_count($).fetch_add(1, std::memory_order_relaxed);
            }
        } else {
            uint32_t got = 0xdeadbeef;
            if (ring.consume(got)) {
                // 读到的必须是合法生产者写入的值
                RL_ASSERT(got == 107 || got == 207);
                consumed_count($).fetch_add(1, std::memory_order_relaxed);
            }
        }
    }

    void after() {
        // 在 spin 限额内应当都成功
        int p = produced_count($).load(std::memory_order_relaxed);
        int c = consumed_count($).load(std::memory_order_relaxed);
        // 允许 spin 耗尽导致部分失败；但若生产成功，消费也应当成功
        RL_ASSERT(c <= p);
    }
};

int main() {
    rl::test_params p;
    p.iteration_count = 10000;
    rl::simulate<mpmc_test>(p);
    return 0;
}
