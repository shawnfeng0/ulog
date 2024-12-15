//
// Created by shawnfeng on 23-3-31.
//

#include "ulog/queue/mpsc_ring.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <random>
#include <thread>

#include "ulog/queue/mpmc_ring.h"

template <typename Mq>
static void mq_test(const size_t buffer_size, const size_t write_thread_count, const size_t publish_count,
                    const size_t read_thread_count) {
  const auto umq = Mq::Create(buffer_size);
  uint8_t data_source[256];
  for (size_t i = 0; i < sizeof(data_source); i++) {
    data_source[i] = i;
  }

  std::atomic_uint64_t total_write_size(0);

  auto write_entry = [=, &total_write_size] {
    typename Mq::Producer producer(umq);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(8, std::min(256UL, buffer_size / 4));

    uint64_t total_num = 0;
    while (total_num++ < publish_count) {
      size_t size = dis(gen);
      const auto data = producer.ReserveOrWaitFor(size, std::chrono::milliseconds(100));
      if (data == nullptr) {
        continue;
      }
      memcpy(data, data_source, std::min(sizeof(data_source), size));
      producer.Commit(size);
      total_write_size += size;
    }
    producer.Flush();
  };

  std::vector<std::thread> write_thread;
  for (size_t i = 0; i < write_thread_count; ++i) write_thread.emplace_back(write_entry);

  std::atomic_size_t total_read_packet(0);
  std::atomic_size_t total_read_size(0);
  auto read_entry = [=, &total_read_packet, &total_read_size] {
    typename Mq::Consumer consumer(umq);

    while (auto data = consumer.ReadOrWait(std::chrono::milliseconds(1000))) {
      total_read_packet += data.remain();
      while (const auto packet = data.next()) {
        // ASSERT_EQ(memcmp(data_source, packet.data, packet.size), 0);
        if (memcmp(data_source, packet.data, packet.size) != 0) {
          consumer.Debug();
        }
        total_read_size += packet.size;
      }
      consumer.ReleasePacket(data);
    }
  };

  std::vector<std::thread> read_thread;
  for (size_t i = 0; i < read_thread_count; ++i) read_thread.emplace_back(read_entry);

  for (auto& t : write_thread) t.join();
  for (auto& t : read_thread) t.join();

  ASSERT_EQ(total_read_packet, publish_count * write_thread_count);
  ASSERT_EQ(total_read_size, total_write_size);
  LOGGER_MULTI_TOKEN(total_write_size.load());
}

TEST(MpscRingTest, multi_producer_single_consumer) { mq_test<ulog::mpsc::Mq>(1024, 4, 1024 * 100, 1); }

TEST(MpscRingTest, multi_producer_multi_consumer) { mq_test<ulog::mpmc::Mq>(1024, 4, 1024 * 100, 1); }
