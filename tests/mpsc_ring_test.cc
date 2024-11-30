//
// Created by shawnfeng on 23-3-31.
//

#include "ulog/helper/queue/mpsc_ring.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <random>
#include <thread>

static void umq_mpsc(const size_t buffer_size, const size_t max_write_thread, const size_t publish_count) {
  const auto umq = ulog::umq::Umq::Create(buffer_size);
  constexpr char data_source[] =
      "1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 "
      "36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 51 52 53 54 55 56 57 58 59 60 61 62 63 64 65 66 67 68 69 70 71 "
      "72 73 74 75 76 77 78 79 80 81 82 83 84 85 86 87 88 89 90 91 92 93 94 95 96 97 98 99";
  std::atomic_uint64_t total_write_size{0};

  auto write_entry = [=, &total_write_size] {
    ulog::umq::Producer producer(umq);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(8, 256);

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
  for (size_t i = 0; i < max_write_thread; ++i) write_thread.emplace_back(write_entry);

  std::thread read_thread{[=, &total_write_size] {
    ulog::umq::Consumer consumer(umq);
    size_t total_packet = 0;
    size_t total_size = 0;
    while (ulog::umq::DataPacket ptr = consumer.ReadOrWait(std::chrono::milliseconds(10))) {
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

TEST(MpscRingTest, multi_producer_single_consumer) { umq_mpsc(64 * 1024, 16, 100 * 1024); }
