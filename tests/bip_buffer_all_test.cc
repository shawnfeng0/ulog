//
// Created by shawnfeng on 23-3-31.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <random>
#include <thread>

#include "ulog/helper/queue/bip_buffer.h"
#include "ulog/helper/queue/bip_buffer2.h"
#include "ulog/ulog.h"

TEST(BipBuffer, IsInRange) {
  ASSERT_EQ(ulog::queue::IsInRange(10, 9, 20), false);
  ASSERT_EQ(ulog::queue::IsInRange(10, 10, 20), true);
  ASSERT_EQ(ulog::queue::IsInRange(10, 11, 20), true);

  ASSERT_EQ(ulog::queue::IsInRange(10, 19, 20), true);
  ASSERT_EQ(ulog::queue::IsInRange(10, 20, 20), true);
  ASSERT_EQ(ulog::queue::IsInRange(10, 21, 20), false);

  ASSERT_EQ(ulog::queue::IsInRange(10, 9, 10), false);
  ASSERT_EQ(ulog::queue::IsInRange(10, 10, 10), true);
  ASSERT_EQ(ulog::queue::IsInRange(10, 11, 10), false);

  ASSERT_EQ(ulog::queue::IsInRange(UINT32_MAX - 1, UINT32_MAX - 2, 1), false);
  ASSERT_EQ(ulog::queue::IsInRange(UINT32_MAX - 1, UINT32_MAX - 1, 1), true);
  ASSERT_EQ(ulog::queue::IsInRange(UINT32_MAX - 1, UINT32_MAX, 1), true);
  ASSERT_EQ(ulog::queue::IsInRange(UINT32_MAX - 1, 0, 1), true);
  ASSERT_EQ(ulog::queue::IsInRange(UINT32_MAX - 1, 1, 1), true);
  ASSERT_EQ(ulog::queue::IsInRange(UINT32_MAX - 1, 2, 1), false);
}

template <typename T>
static void spsc(uint32_t buffer_size) {
  const uint32_t limit = buffer_size * 8192;
  T buffer(buffer_size);

  std::thread write_thread{[&] {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(
        1, std::max<uint32_t>(buffer_size / 10, 2));
    uint32_t write_count = 0;
    while (write_count < limit) {
      size_t size = dis(gen);
      auto data = buffer.TryReserve(size);
      if (data == nullptr) {
        std::this_thread::yield();
        continue;
      }
      for (size_t i = 0; i < size; ++i) data[i] = write_count++;
      buffer.Commit(size);
    }
  }};

  std::thread read_thread{[&] {
    uint32_t read_count = 0;
    while (read_count < limit) {
      size_t size;
      auto data = buffer.TryRead(&size);
      if (data == nullptr) {
        std::this_thread::yield();
        continue;
      }
      for (size_t i = 0; i < size; ++i) {
        ASSERT_EQ(data[i], read_count++);
      }
      buffer.Release(size);
    }
  }};

  write_thread.join();
  read_thread.join();
  printf("Finished test: buffer_size: %u, limit: %u\n", buffer_size, limit);
}

TEST(BipBufferTestSingle, singl_producer_single_consumer) {
  for (uint32_t i = 4; i < 10; ++i) {
    LOGGER_TIME_CODE({ spsc<ulog::BipBuffer<uint32_t>>(1 << i); });
    LOGGER_TIME_CODE({ spsc<ulog::BipBuffer2<uint32_t>>(1 << i); });
  }
}
