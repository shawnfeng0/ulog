#include "ulog/queue/mpmc_ring.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <random>
#include <thread>
#include <vector>

using Mq = ulog::mpmc::Mq;

// ── Single-threaded basic tests ─────────────────────────────────────────

TEST(MpmcRingTest, basic) {
  const auto umq = Mq::Create(1024);
  Mq::Producer producer(umq);
  Mq::Consumer consumer(umq);

  uint8_t data[] = "hello";
  auto p = producer.Reserve(sizeof(data));
  ASSERT_NE(p, nullptr);
  memcpy(p, data, sizeof(data));
  producer.Commit(p, sizeof(data));

  auto rd = consumer.Read();
  ASSERT_TRUE(rd);
  auto pkt = rd.next();
  ASSERT_EQ(pkt.size, sizeof(data));
  ASSERT_EQ(memcmp(pkt.data, data, sizeof(data)), 0);
  consumer.Release(rd);
}

TEST(MpmcRingTest, multiple_writes_reads) {
  const auto umq = Mq::Create(1024);
  Mq::Producer producer(umq);
  Mq::Consumer consumer(umq);

  for (int i = 0; i < 50; i++) {
    auto val = static_cast<uint8_t>(i);
    auto p = producer.Reserve(1);
    ASSERT_NE(p, nullptr);
    *p = val;
    producer.Commit(p, 1);

    auto rd = consumer.Read();
    ASSERT_TRUE(rd);
    auto pkt = rd.next();
    ASSERT_EQ(pkt.size, 1u);
    ASSERT_EQ(*pkt.data, val);
    consumer.Release(rd);
  }
}

TEST(MpmcRingTest, empty_queue) {
  const auto umq = Mq::Create(1024);
  Mq::Consumer consumer(umq);

  auto rd = consumer.Read();
  ASSERT_FALSE(rd);
  ASSERT_EQ(rd.remain(), 0u);
}

TEST(MpmcRingTest, queue_full) {
  const auto umq = Mq::Create(32);
  Mq::Producer producer(umq);

  int count = 0;
  uint8_t *p;
  while ((p = producer.Reserve(8)) != nullptr) {
    producer.Commit(p, 8);
    count++;
    if (count > 100) break;
  }
  ASSERT_GT(count, 0);
  ASSERT_EQ(producer.Reserve(8), nullptr);
}

TEST(MpmcRingTest, fill_drain_cycle) {
  const auto umq = Mq::Create(256);
  Mq::Producer producer(umq);
  Mq::Consumer consumer(umq);

  uint8_t source[16];
  memset(source, 0xAB, sizeof(source));

  for (int cycle = 0; cycle < 10; cycle++) {
    int written = 0;
    while (auto p = producer.Reserve(sizeof(source))) {
      memcpy(p, source, sizeof(source));
      producer.Commit(p, sizeof(source));
      written++;
      if (written > 100) break;
    }
    ASSERT_GT(written, 0);

    int read_count = 0;
    while (auto rd = consumer.Read()) {
      while (auto pkt = rd.next()) {
        ASSERT_EQ(pkt.size, sizeof(source));
        ASSERT_EQ(memcmp(pkt.data, source, sizeof(source)), 0);
        read_count++;
      }
      consumer.Release(rd);
    }
    ASSERT_EQ(read_count, written);
  }
}

TEST(MpmcRingTest, varying_packet_sizes) {
  const auto umq = Mq::Create(4096);
  Mq::Producer producer(umq);
  Mq::Consumer consumer(umq);

  uint8_t source[256];
  for (size_t i = 0; i < sizeof(source); i++) source[i] = i;

  std::vector<size_t> sizes = {1, 2, 7, 8, 15, 16, 31, 32, 63, 64, 100, 200};
  for (auto sz : sizes) {
    auto p = producer.Reserve(sz);
    ASSERT_NE(p, nullptr) << "Failed to reserve size " << sz;
    memcpy(p, source, sz);
    producer.Commit(p, sz);
  }

  size_t idx = 0;
  while (auto rd = consumer.Read()) {
    while (auto pkt = rd.next()) {
      ASSERT_LT(idx, sizes.size());
      ASSERT_EQ(pkt.size, sizes[idx]);
      ASSERT_EQ(memcmp(pkt.data, source, sizes[idx]), 0);
      idx++;
    }
    consumer.Release(rd);
  }
  ASSERT_EQ(idx, sizes.size());
}

TEST(MpmcRingTest, discard_packet) {
  const auto umq = Mq::Create(4096);
  Mq::Producer producer(umq);
  Mq::Consumer consumer(umq);

  // Normal packet
  auto data1 = producer.Reserve(16);
  ASSERT_NE(data1, nullptr);
  std::memset(data1, 0xAA, 16);
  producer.Commit(data1, 16);

  // Discarded packet (commit with size 0)
  auto data2 = producer.Reserve(32);
  ASSERT_NE(data2, nullptr);
  producer.Commit(data2, 0);

  // Another normal packet
  auto data3 = producer.Reserve(24);
  ASSERT_NE(data3, nullptr);
  std::memset(data3, 0xBB, 24);
  producer.Commit(data3, 24);

  auto rdata = consumer.Read();
  ASSERT_TRUE(rdata);

  int valid_count = 0;
  while (auto p = rdata.next()) {
    if (p.size > 0) valid_count++;
  }
  ASSERT_EQ(valid_count, 2);
  consumer.Release(rdata);
}

// ── Blocking / timeout tests ────────────────────────────────────────────

TEST(MpmcRingTest, read_or_wait_timeout) {
  const auto umq = Mq::Create(1024);
  Mq::Consumer consumer(umq);

  auto start = std::chrono::steady_clock::now();
  auto rd = consumer.ReadOrWait(std::chrono::milliseconds(50));
  auto elapsed = std::chrono::steady_clock::now() - start;

  ASSERT_FALSE(rd);
  ASSERT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 40);
}

TEST(MpmcRingTest, reserve_or_wait_unblocks) {
  const auto umq = Mq::Create(64);
  Mq::Producer producer(umq);
  Mq::Consumer consumer(umq);

  // Fill the buffer
  uint8_t *fill_p;
  while ((fill_p = producer.Reserve(8)) != nullptr) {
    producer.Commit(fill_p, 8);
  }

  // In a separate thread, consume after a delay
  std::thread releaser([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    auto rd = consumer.Read();
    if (rd) consumer.Release(rd);
  });

  // Should block and then succeed after the consumer releases
  auto p = producer.ReserveOrWaitFor(8, std::chrono::milliseconds(500));
  ASSERT_NE(p, nullptr);
  producer.Commit(p, 8);

  releaser.join();
}

// ── Multi-threaded stress tests ─────────────────────────────────────────

static void mpmc_stress_test(size_t buffer_size, size_t write_thread_count,
                             size_t publish_count_per_thread, size_t read_thread_count) {
  const auto umq = Mq::Create(buffer_size);
  uint8_t data_source[256];
  for (size_t i = 0; i < sizeof(data_source); i++) data_source[i] = i;

  std::atomic<uint64_t> total_write_size{0};
  std::atomic<uint64_t> total_write_packets{0};

  auto write_entry = [&] {
    Mq::Producer producer(umq);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(8, std::min<size_t>(200, buffer_size / 4));

    for (size_t n = 0; n < publish_count_per_thread; n++) {
      size_t sz = dis(gen);
      auto data = producer.ReserveOrWaitFor(sz, std::chrono::milliseconds(200));
      if (data == nullptr) continue;
      memcpy(data, data_source, std::min(sizeof(data_source), sz));
      producer.Commit(data, sz);
      total_write_size += sz;
      total_write_packets++;
    }
    producer.Flush();
  };

  std::atomic<uint64_t> total_read_size{0};
  std::atomic<uint64_t> total_read_packets{0};

  auto read_entry = [&] {
    Mq::Consumer consumer(umq);
    while (auto data = consumer.ReadOrWait(std::chrono::milliseconds(50))) {
      while (auto pkt = data.next()) {
        ASSERT_EQ(memcmp(data_source, pkt.data, std::min(sizeof(data_source), pkt.size)), 0);
        total_read_size += pkt.size;
        total_read_packets++;
      }
      consumer.Release(data);
    }
  };

  std::vector<std::thread> writers, readers;
  for (size_t i = 0; i < read_thread_count; ++i) readers.emplace_back(read_entry);
  for (size_t i = 0; i < write_thread_count; ++i) writers.emplace_back(write_entry);

  for (auto &t : writers) t.join();
  umq->Notify();
  for (auto &t : readers) t.join();

  ASSERT_EQ(total_read_packets.load(), total_write_packets.load());
  ASSERT_EQ(total_read_size.load(), total_write_size.load());
}

TEST(MpmcRingTest, spsc) { mpmc_stress_test(4096, 1, 5000, 1); }
TEST(MpmcRingTest, mpsc_2_producers) { mpmc_stress_test(8192, 2, 5000, 1); }
TEST(MpmcRingTest, mpsc_4_producers) { mpmc_stress_test(16384, 4, 5000, 1); }
TEST(MpmcRingTest, spmc_2_consumers) { mpmc_stress_test(8192, 1, 5000, 2); }
TEST(MpmcRingTest, spmc_4_consumers) { mpmc_stress_test(16384, 1, 5000, 4); }
TEST(MpmcRingTest, mpmc_2x2) { mpmc_stress_test(8192, 2, 5000, 2); }
TEST(MpmcRingTest, mpmc_4x4) { mpmc_stress_test(16384, 4, 2000, 4); }
TEST(MpmcRingTest, mpmc_heavy_contention) { mpmc_stress_test(4096, 8, 1000, 4); }
TEST(MpmcRingTest, small_buffer_mpmc) { mpmc_stress_test(128, 2, 2000, 2); }
