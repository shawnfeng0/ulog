# 无锁队列 Relacy 验证模型

本目录的 `.cpp` 文件是 `include/ulog/queue/*.h` 三种无锁队列核心同步协议的
Relacy Race Detector 版本，用于在弱内存模型下验证 acquire/release 配对和
CAS 协议的正确性。

它们不是源码的直接复用，而是**等价最小模型**（定长 N=2 槽位、纯游标推进、
忽略 Header 中 discarded 标志和变长 payload 等工程细节），专注于复现各队列
的同步骨架。

## 文件

| 文件 | 对应源码 | 说明 | 期望结果 |
|------|---------|------|----------|
| `spsc_model.cpp` | `spsc_ring.h` | 单生产者 + 单消费者，`in_`/`out_` 经典 release/acquire | 10000 iterations 全通过 |
| `mpsc_model.cpp` | `mpsc_ring.h` | 多生产者（CAS `prod_head_`） + 单消费者（release-store `cons_head_`） | 10000 iterations 全通过 |
| `mpmc_model.cpp` | `mpmc_ring.h` | 当前修复后协议：生产者用 `cons_tail_(acquire)`、消费者按 claim 顺序 release `cons_tail_` | 10000 iterations 全通过 |
| `mpmc_model_buggy.cpp` | 历史 BUG 版 | 生产者用 `cons_head_` 判空间，没有 `cons_tail_`；与消费者 memset 竞态 | Relacy 报 **LIVELOCK** |
| `notifier_model.cpp` | `lite_notifier.h` 修复后 | `wait`/`notify_*` 中用 `atomic_thread_fence(seq_cst)` 配对 | 100000 iterations 全通过 |
| `notifier_model_buggy.cpp` | `lite_notifier.h` 历史 BUG 版 | 仅用 release/acquire，缺 StoreLoad 屏障，典型 Dekker 问题 | Relacy 报 **DEADLOCK**（第 6 次迭代即可复现） |
| `eventcount_model.cpp` | `eventcount.h` | 基于 epoch 计数器，seq_cst 直接加在 `epoch_`/`waiters_` 原子操作上，不用 `atomic_thread_fence` | 100000 iterations 全通过 |

## 协议要点

- **SPSC**：payload 写入 → `in_.store(release)` 与 `in_.load(acquire)` → 读取，形成 happens-before；对称地 `out_` 保护空间
- **MPSC**：多生产者 CAS 序列化 claim；消费者单点，`cons_head_.store(release)` 发布空间
- **MPMC**：相比 MPSC 多了一个 `cons_tail_`，因为**多消费者 claim 顺序 ≠ memset 完成顺序**，必须用独立原子按 claim 顺序公布"已真正释放"的位置
- **LiteNotifier**：快速路径 `if (waiters_==0) skip notify` 本质是跨变量的 StoreLoad 同步（producer 写 state → load waiters；consumer 写 waiters → load state），只有 `seq_cst` 全局序才能防止双方"同时错过"。实现中用 `atomic_thread_fence(seq_cst)` 在 `wait` 的 fetch_add 之后、`notify_*` 的 waiters load 之前建立该屏障。
- **Eventcount**：将跨变量 StoreLoad 改为 `waiters_` ↔ `epoch_` 之间的 SC，所有 seq_cst 直接落在这两个原子的 fetch_add / load 上；外部 state 的可见性改由"state 写在 epoch 递增之前 (release) / 读在 epoch 读之后 (acquire)"承担，因此不再需要独立 fence。

## 构建与运行

```bash
git clone --depth 1 https://github.com/dvyukov/relacy.git /tmp/relacy

for m in spsc_model mpsc_model mpmc_model mpmc_model_buggy \
         notifier_model notifier_model_buggy eventcount_model; do
    g++ -std=c++14 -O1 -I/tmp/relacy \
        -Wno-deprecated -Wno-unused-parameter \
        $m.cpp -o /tmp/$m
done

/tmp/spsc_model              # iterations: 10000
/tmp/mpsc_model              # iterations: 10000
/tmp/mpmc_model              # iterations: 10000
/tmp/mpmc_model_buggy        # LIVELOCK（预期）
/tmp/notifier_model          # iterations: 100000
/tmp/notifier_model_buggy    # DEADLOCK（预期）
/tmp/eventcount_model        # iterations: 100000
```

Relacy 实现了 C++11 宽松内存模型，在单进程内穷举所有合法的线程交错
+ 读写可见性组合。若有 race、死锁、断言失败，会打印**完整的事件
序列**（谁在哪一步读到了什么值），便于人工 review。

