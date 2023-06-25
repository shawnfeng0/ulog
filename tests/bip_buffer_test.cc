//
// Created by shawnfeng on 23-3-31.
//

#include "ulog/helper/bip_buffer.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <thread>

TEST(BipBuffer, IsInRange) {
  ASSERT_EQ(IsInRange(10, 9, 20), false);
  ASSERT_EQ(IsInRange(10, 10, 20), true);
  ASSERT_EQ(IsInRange(10, 11, 20), true);

  ASSERT_EQ(IsInRange(10, 19, 20), true);
  ASSERT_EQ(IsInRange(10, 20, 20), true);
  ASSERT_EQ(IsInRange(10, 21, 20), false);

  ASSERT_EQ(IsInRange(10, 9, 10), false);
  ASSERT_EQ(IsInRange(10, 10, 10), true);
  ASSERT_EQ(IsInRange(10, 11, 10), false);

  ASSERT_EQ(IsInRange(UINT32_MAX - 1, UINT32_MAX - 2, 1), false);
  ASSERT_EQ(IsInRange(UINT32_MAX - 1, UINT32_MAX - 1, 1), true);
  ASSERT_EQ(IsInRange(UINT32_MAX - 1, UINT32_MAX, 1), true);
  ASSERT_EQ(IsInRange(UINT32_MAX - 1, 0, 1), true);
  ASSERT_EQ(IsInRange(UINT32_MAX - 1, 1, 1), true);
  ASSERT_EQ(IsInRange(UINT32_MAX - 1, 2, 1), false);
}

static void single_producer_single_consumer(uint32_t write_step,
                                            uint32_t buffer_size) {
  const uint32_t limit = buffer_size * 32;
  BipBuffer<uint32_t> buffer(buffer_size);

  std::thread write_thread{[&] {
    uint32_t write_count = 0;
    while (write_count < limit) {
      size_t size = write_step;
      auto data = buffer.Reserve(size);
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
      auto data = buffer.Read(&size);
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
  printf("Finished test: write_step: %u, buffer_size: %u, limit: %u\n",
         write_step, buffer_size, limit);
}

TEST(BipBufferTestSingle, singl_producer_single_consumer) {
  single_producer_single_consumer(1, 1 << 8);
  single_producer_single_consumer(2, 1 << 8);
  single_producer_single_consumer(10, 1 << 8);
  single_producer_single_consumer(16, 1 << 8);

  single_producer_single_consumer(1, 1 << 16);
  single_producer_single_consumer(2, 1 << 16);
  single_producer_single_consumer(5, 1 << 16);
  single_producer_single_consumer(256, 1 << 16);
  single_producer_single_consumer(1000, 1 << 16);
  single_producer_single_consumer(4000, 1 << 16);
  single_producer_single_consumer(4096, 1 << 16);
}
