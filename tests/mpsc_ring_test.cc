//
// Created by shawnfeng on 23-3-31.
//

#include "ulog/queue/mpsc_ring.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <random>
#include <thread>

static void umq_mpsc(const size_t buffer_size, const size_t max_write_thread, const size_t publish_count) {
  const auto umq = ulog::umq::Umq::Create(buffer_size);
  uint8_t data_source[256];
  for (size_t i = 0; i < sizeof(data_source); i++) {
    data_source[i] = i;
  }

  std::atomic_uint64_t total_write_size{0};

  auto write_entry = [=, &total_write_size] {
    ulog::umq::Producer producer(umq);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(8, std::min(256UL, buffer_size / 4));

    uint64_t total_num = 0;
    while (total_num++ < publish_count) {
      size_t size = dis(gen);
      const auto data = producer.ReserveOrWaitFor(size, std::chrono::milliseconds(1000));
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
  for (size_t i = 0; i < max_write_thread; ++i) write_thread.emplace_back(write_entry);

  std::thread read_thread{[=, &total_write_size] {
    ulog::umq::Consumer consumer(umq);
    size_t total_packet = 0;
    size_t total_size = 0;

    while (ulog::umq::DataPacket ptr = consumer.ReadOrWait(std::chrono::milliseconds(1000))) {
      total_packet += ptr.remain();
      while (const auto data = ptr.next()) {
        ASSERT_EQ(memcmp(data_source, data.data, data.size), 0);
        total_size += data.size;
      }
      consumer.ReleasePacket();
    }
    ASSERT_EQ(total_packet, publish_count * max_write_thread);
    ASSERT_EQ(total_size, total_write_size);
    LOGGER_MULTI_TOKEN(total_write_size.load());
  }};

  for (auto& t : write_thread) t.join();
  read_thread.join();
}

TEST(MpscRingTest, multi_producer_single_consumer) { umq_mpsc(1024, 4, 100 * 1024); }
