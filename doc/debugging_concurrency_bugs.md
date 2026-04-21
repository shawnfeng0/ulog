# 并发队列 Bug 排查方法总结

本文以修复 MPMC ring buffer 中 `memcmp != 0` 的数据竞争为例，总结无锁队列 / 多线程代码的排查思路与工具使用方法。

## 一、问题现象

测试偶发失败（非必现）：

```
tests/mpmc_ring_test.cc:249: Failure
  memcmp(data_source, pkt.data, ...) Which is: 1  (期望 0)
tests/mpmc_ring_test.cc:265: Failure
  total_read_packets = 1042  vs  total_write_packets = 1045
```

特征：
- 读取的数据中间混入零字节
- 读包数少于写包数
- 单次运行难复现，`small_buffer_mpmc`（缓冲 128、2 生产 2 消费）最容易触发

这类"偶发 + 数据被部分清零"的现象几乎一定是**数据竞争**。

---

## 二、排查思路（按代价从低到高）

### 1. 稳定复现：压力循环 + 失败即停

在得到诊断工具输出前，先让 bug 频繁出现：

```bash
# 反复跑最小化的 stress 用例，首次失败立即停
./tests/mpmc_ring_test \
    --gtest_filter='*small_buffer*:*heavy*' \
    --gtest_repeat=100 \
    --gtest_break_on_failure
```

要点：
- 选择**小缓冲、多线程**的用例放大竞争概率
- `--gtest_break_on_failure` 保留首个失败现场
- 一定加**超时**（`timeout 60 ...` 或外层 CI 超时），无锁代码 bug 常导致死循环 / 死锁，不设超时会挂死

### 2. ThreadSanitizer（TSAN）——首选

TSAN 是 Google 开发、集成在 Clang/GCC 中的动态数据竞争检测器。原理是在编译期为每一条 load/store/atomic 插桩，运行时维护一个"向量时钟"跟踪每块内存的 happens-before 关系。一旦检测到两个线程对同一地址的访问**没有**同步关系（且至少一方是写），立刻输出告警。

特点：
- **低误报**：TSAN 基于 happens-before，不是基于 lockset，几乎不会误报
- **只查看实际跑到的时间线**：没跑到的路径不会报（所以要配合压力测试）
- **运行时开销**：内存 5~10 倍、速度 5~15 倍，不适合生产环境
- **限制**：需要重编译；程序里所有库最好都带 TSAN 插桩，否则第三方 so 里的同步 TSAN 看不到，可能误报
- 不能和 ASan 同时启用

#### 构建

```bash
cmake -S . -B build-tsan \
    -DCMAKE_BUILD_TYPE=Debug \
    -DULOG_BUILD_TESTS=ON \
    -DCMAKE_CXX_FLAGS="-fsanitize=thread -g -O1" \
    -DCMAKE_C_FLAGS="-fsanitize=thread -g -O1" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"
cmake --build build-tsan -j
```

#### 运行

```bash
timeout 120 ./build-tsan/tests/mpmc_ring_test \
    --gtest_filter='*small_buffer*' \
    --gtest_repeat=20
```

#### 读懂输出

TSAN 报告形如：

```
WARNING: ThreadSanitizer: data race
  Write of size 1 at 0x... by thread T3:
    #0 memset
    #1 Consumer::Release  include/ulog/queue/mpmc_ring.h:448
  Previous read of size 1 at 0x... by thread T1:
    #0 IsAllZero           include/ulog/queue/power_of_two.h:...
    #1 Producer::Reserve   include/ulog/queue/mpmc_ring.h:265
```

关键信息：
- **两条堆栈分别是冲突的双方**（写 vs 读）
- **代码行号直接可定位**
- 对应上下文推断：消费者在 Release 清零的同时，生产者正按字节扫 `IsAllZero` —— 这就是 race

> 小技巧：TSAN 下程序会慢 5~10 倍，天然放大竞争窗口，所以有时 TSAN 能复现、普通 Debug 反而不容易复现。

### 3. AddressSanitizer / UBSan

- **ASan**（Address Sanitizer）：在每块分配前后放置"红区（redzone）"并为已释放内存染色（quarantine），运行时检查每一次 load/store 是否落在合法区域。能发现**堆/栈/全局越界、use-after-free、double free、内存泄漏**。开销约 2~3 倍。
- **UBSan**（Undefined Behavior Sanitizer）：一组小插桩的集合，检测 C/C++ 中的未定义行为，例如有符号整数溢出、移位越界、空指针解引用、非法枚举值、错位访问等。开销很低，可长期常开。

两者可以同时启用（`-fsanitize=address,undefined`），但**不能**和 TSAN 同时启用。常见做法：CI 跑三套构建，TSAN / ASan+UBSan / 普通 Release。

### 4. 日志 / printf 插桩

在 CAS、store、memset 处打印 `(tid, offset, size)`，输出到内存 ring buffer 或 `/tmp/log`（避免 I/O 影响时序）。适合：
- TSAN 报告不明确时追踪状态机
- 验证修复假设（事件顺序是否符合预期）

建议只开 Debug 编译，默认关闭。

### 5. 静态模型检查 / 形式化验证

这类工具和前面的运行时 sanitizer 有本质区别：TSAN 只观察**实际跑到的那一条时间线**，而模型检查会**系统地穷举所有可能的线程交错 / 内存可见性组合**。代价是要么写 harness、要么重写抽象模型。

#### 5.1 CDSChecker / C11Tester

- **类型**：C/C++11 无锁数据结构专用的模型检查器
- **原理**：自己实现一套 C++11 宽松内存模型（relaxed/acquire/release/seq_cst）的解释器，在用户态模拟调度，穷举不同的读写可见性组合
- **能发现什么**：仅在 ARM/POWER 等弱内存序平台才会暴露的 bug。比如你把某个 `release` 写成了 `relaxed`，x86 上可能永远跑不出问题，但 CDSChecker 能直接报出来
- **使用方式**：
  1. 把无锁数据结构单独编译成带 CDSChecker runtime 的库
  2. 写一个**极简 harness**（通常 2~4 个线程，每线程做 1~2 次操作）
  3. 工具驱动跑，几秒到几分钟内报告所有合法但会导致断言失败的交错
- **局限**：harness 规模要小，线程 / 操作一多状态爆炸
- **主页**：<http://plrg.eecs.uci.edu/c11tester/>（C11Tester 是 CDSChecker 的后继，增加了 LLVM pass，接入更平滑）

> 对本仓库 `mpmc_ring.h` 的 acquire/release 配对是否正确，CDSChecker 是最直接的验证工具 —— TSAN 无法证明"内存序选得够强"，它只能检测当前这次运行的竞争。

#### 5.2 Relacy Race Detector

- **类型**：C++ header-only 的用户态模型检查器
- **原理**：提供 `std::atomic` / `std::mutex` / `std::thread` 的同名替身，用宏把你代码里的并发原语替换掉。运行时 Relacy 在**单进程内**调度"伪线程"，穷举所有交错顺序；同时它也实现了弱内存模型，能模拟 reorder
- **特点**：
  - 不需要改工具链，只要 include 头文件
  - 报告极其直观：输出一个交错序列表格，逐步列出"第 N 步，线程 X 执行了哪条语句，读到的值是多少"，适合人工 review
  - 速度快，几秒覆盖数万种交错
- **局限**：
  - 项目长期未维护，C++17/20 下编译要打补丁
  - 要把被测代码包进 `RL_TEST` 宏块，对现有代码入侵较大
  - 线程数 > 4 后状态空间迅速爆炸
- **仓库**：<https://github.com/dvyukov/relacy>（作者 Dmitry Vyukov 是 Google 并发专家，也是 TSAN v2 的设计者之一）

> 典型用法：为队列写一个 `RL_TEST`（2 producer + 2 consumer 各做一次 enqueue/dequeue），几秒就能覆盖所有交错。适合**合入新无锁算法前的兜底验证**。
>
> **本仓库已在 `tools/relacy_model/` 下维护了三种队列的 Relacy 模型**：
> - `spsc_model.cpp` / `mpsc_model.cpp` / `mpmc_model.cpp`：10000 次迭代全通过
> - `mpmc_model_buggy.cpp`：修复前协议（生产者用 `cons_head_` 判空间），Relacy 直接报 LIVELOCK
> 详见 `tools/relacy_model/README.md`。

#### 5.3 TLA+

- **类型**：形式化规约语言 + 模型检查器，**不运行你的 C++ 代码**
- **作者**：Leslie Lamport（2013 年图灵奖得主，也是 LaTeX 和 Paxos 的作者）
- **原理**：用数学语言（基于集合论和时态逻辑）描述算法的**状态机**：有哪些变量、每一步的动作如何改变变量、初始状态是什么。然后 TLC 模型检查器枚举所有可达状态，验证你声明的**不变量（Safety）**永远成立、**活性（Liveness）**最终达成
- **特点**：
  - 验证的是**算法 / 协议层**的正确性，与实现语言、平台、甚至具体数据结构都无关
  - 能发现规约层面的设计 bug，而这类 bug 在代码里往往要跑数月才遇到一次
  - 工业界案例：AWS 用 TLA+ 验过 S3、DynamoDB 的一致性协议；MongoDB、etcd、CockroachDB 都有公开的 TLA+ 规约
- **工具链**：
  - TLA+ Toolbox（官方 IDE）
  - TLC（显式状态模型检查器，默认用这个）
  - Apalache（符号模型检查器，能处理更大状态空间）
  - PlusCal（一种类伪代码语法，编译成 TLA+，降低入门门槛）
- **学习成本**：相对较高，需要接受"不写代码而写规约"的思维方式
- **资源**：
  - 官方：<https://lamport.azurewebsites.net/tla/tla.html>
  - 入门推荐：Hillel Wayne 的 *Learn TLA+*（<https://learntla.com/>）

> 对 ring buffer 这种单机数据结构，TLA+ 收益较低；但如果仓库未来引入**分布式日志同步 / 多节点协议**，强烈推荐用 TLA+ 先把协议规约清楚。

#### 5.4 工具对比

| 工具 | 验证对象 | 需要改代码？ | 探索方式 | 覆盖弱内存序 | 典型用途 |
|------|---------|-----------|---------|------------|---------|
| TSAN | 实际 C++ 代码 | 否（重编译即可） | 观察一次真实运行 | 否（只看 x86 实际行为） | 捕捉当次运行发生的 race |
| ASan/UBSan | 实际 C++ 代码 | 否 | 观察一次真实运行 | 不适用 | 内存错误、UB |
| CDSChecker | 实际 C++11 代码 | 写 harness | 穷举弱内存序下的可见性 | **是** | 验证 memory_order 选取 |
| Relacy | 实际 C++ 代码（替换头文件） | 轻度改造 | 穷举线程交错 + 弱内存序 | **是** | 无锁结构快速验证 |
| TLA+ | 抽象状态机 | 重写规约 | 状态空间穷举 | 不适用（规约层） | 协议 / 算法正确性

### 6. 放大竞争的小技巧

在相关调用点手动插入 `std::this_thread::yield()` 或 `usleep(1)`，让调度点"卡"在可疑位置，普通 Debug 下也能放大 race 概率。例如把 `memset` 前后各 yield 一次往往立刻复现。

---

## 三、本次 MPMC race 的完整排查链路

1. **复现**：用 `--gtest_filter='*small_buffer*' --gtest_repeat=100 --gtest_break_on_failure`，第 10 次复现
2. **TSAN 定位**：报告两处冲突
   - Writer：`Consumer::Release` 中的 `memset`（mpmc_ring.h:448）
   - Reader：`Producer::Reserve` 中的 `IsAllZero`（mpmc_ring.h:265）
3. **根因分析**：
   - `Read()` 里 CAS 立即推进 `cons_head_`
   - `memset` 在之后的 `Release()` 里才执行
   - 生产者看到新的 `cons_head_`，以为空间可用，通过 `IsAllZero` 字节扫描判断是否可覆盖 —— 与 `memset` 字节写发生 race
4. **修复**：参考 MPSC 的做法，引入第二个原子 `cons_tail_`：
   - `cons_head_`：claim 指针，消费者 CAS 推进
   - `cons_tail_`：free 指针，消费者在 **memset 完成后** release-store
   - 生产者用 `cons_tail_.load(acquire)` 判空间 —— 与消费者 store 形成 release/acquire 配对
   - 删除不可靠的 `IsAllZero` 检查
5. **验证**：
   - 非 TSAN：`small_buffer_mpmc × 500`、全套 `× 200` 全通过
   - TSAN：18 tests × 10 iterations 全通过

---

## 四、排查清单（checklist）

遇到无锁队列偶发数据错误 / 计数不一致时，按顺序过：

- [ ] 用最小压力用例 + `--gtest_repeat` + `--gtest_break_on_failure` 稳定复现
- [ ] 外层加 `timeout`，防死锁卡死
- [ ] 构建 TSAN 版本，跑同样的用例
- [ ] 阅读 TSAN 的 Write/Read 双堆栈，定位冲突代码行
- [ ] 找出竞争的**同步点**：该访问是否应该由某个原子操作保护？release/acquire 是否配对？
- [ ] 若 TSAN 报告不清晰，辅以插桩日志 / yield 放大
- [ ] 修复后回归：TSAN + 长循环压力双重验证

---

## 五、参考

- TSAN 手册：<https://clang.llvm.org/docs/ThreadSanitizer.html>
- C++ memory order：<https://en.cppreference.com/w/cpp/atomic/memory_order>
- 本仓库 MPSC 参考实现：`include/ulog/queue/mpsc_ring.h`
- 修复后的 MPMC：`include/ulog/queue/mpmc_ring.h`
