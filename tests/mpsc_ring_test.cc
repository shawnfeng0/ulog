//
// Created by shawnfeng on 23-3-31.
//

#include "ulog/helper/queue/mpsc_ring.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <random>
#include <thread>

#include "ulog/ulog.h"

static void spsc(uint32_t buffer_size) {
  const uint32_t limit = buffer_size * 8192;
  ulog::umq::Umq<uint32_t> buffer(buffer_size);

  std::thread write_thread{[&] {
    ulog::umq::Producer<uint32_t> producer(&buffer);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(
        1, std::max<uint32_t>(buffer_size / 10, 2));
    uint32_t write_count = 0;
    while (write_count < limit) {
      size_t size = dis(gen);
      auto data = producer.TryReserve(size);
      if (data == nullptr) {
        std::this_thread::yield();
        continue;
      }
      for (size_t i = 0; i < size; ++i) data[i] = write_count++;
      producer.Commit(size);
    }
  }};

  std::thread read_thread{[&] {
    ulog::umq::Consumer<uint32_t> consumer(&buffer);
    uint32_t read_count = 0;
    while (read_count < limit) {
      size_t size;
      auto data = consumer.TryRead(&size);
      if (data == nullptr) {
        std::this_thread::yield();
        continue;
      }
      for (size_t i = 0; i < size; ++i) {
        ASSERT_EQ(data[i], read_count++);
      }
      consumer.Release(size);
    }
  }};

  write_thread.join();
  read_thread.join();
  printf("Finished test: buffer_size: %u, limit: %u\n", buffer_size, limit);
}

TEST(MpscRingTest, singl_producer_single_consumer) {
  for (uint32_t i = 4; i < 10; ++i) {
    LOGGER_TIME_CODE({ spsc(1 << i); });
  }
}
