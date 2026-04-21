// Relacy 模型：复刻 ulog spsc_ring.h 的同步协议（简化版）
// - 单生产者 / 单消费者，定长 N 槽
// - in_  (atomic)  = producer 写游标
// - out_ (atomic)  = consumer 读游标
// - 生产者：load out_(acquire) 判空间 -> 写 payload -> store in_(release)
// - 消费者：load in_(acquire) 判有数据 -> 读 payload -> store out_(release)
// 不变量：消费者读到的 payload 必须等于生产者写入的值
#include "../relacy/relacy/relacy_std.hpp"
#include <cstdint>

static constexpr uint32_t N = 2;

struct SpscRing {
    VAR_T(uint32_t) slots[N];
    std::atomic<uint32_t> in_;
    std::atomic<uint32_t> out_;

    void init() {
        for (uint32_t i = 0; i < N; ++i) VAR(slots[i]) = 0;
        in_($).store(0, std::memory_order_relaxed);
        out_($).store(0, std::memory_order_relaxed);
    }

    bool produce(uint32_t value) {
        uint32_t in  = in_($).load(std::memory_order_relaxed);
        uint32_t out = out_($).load(std::memory_order_acquire);
        if (in - out >= N) return false;
        VAR(slots[in % N]) = value;
        in_($).store(in + 1, std::memory_order_release);
        return true;
    }

    bool consume(uint32_t& value) {
        uint32_t out = out_($).load(std::memory_order_relaxed);
        uint32_t in  = in_($).load(std::memory_order_acquire);
        if (in == out) return false;
        value = VAR(slots[out % N]);
        out_($).store(out + 1, std::memory_order_release);
        return true;
    }
};

struct spsc_test : rl::test_suite<spsc_test, 2> {
    SpscRing ring;

    void before() { ring.init(); }

    void thread(unsigned tid) {
        if (tid == 0) {
            // 连发两条
            for (uint32_t v = 101; v <= 102; ++v) {
                while (!ring.produce(v)) rl::yield(1, $);
            }
        } else {
            uint32_t expected = 101;
            for (int i = 0; i < 2; ++i) {
                uint32_t got = 0;
                while (!ring.consume(got)) rl::yield(1, $);
                RL_ASSERT(got == expected);
                ++expected;
            }
        }
    }
};

int main() {
    rl::test_params p;
    p.iteration_count = 10000;
    rl::simulate<spsc_test>(p);
    return 0;
}
