//
// Created by shawnfeng on 23-3-31.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <random>
#include <thread>

#include "ulog/helper/queue/spsc_bip_buffer.h"
#include "ulog/helper/queue/spsc_bip_buffer2.h"
#include "ulog/ulog.h"

template <typename T>
static void spsc(uint32_t buffer_size) {
  const uint64_t limit = buffer_size * 8192;
  T buffer(buffer_size);

  std::thread write_thread{[&] {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(1, std::max<uint32_t>(buffer_size / 100, 2));
    uint64_t write_count = 0;
    while (write_count < limit) {
      size_t size = dis(gen);

      decltype(buffer.TryReserve(size)) data;
      while ((data = buffer.TryReserve(size)) == nullptr) {
        std::this_thread::yield();
      }
      for (size_t i = 0; i < size; ++i) data[i] = write_count++;
      buffer.Commit(size);
    }
  }};

  std::thread read_thread{[&] {
    uint64_t read_count = 0;
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
  printf("Finished test: buffer_size: %u, limit: %" PRIu64 "\n", buffer_size, limit);
}

TEST(BipBufferTestSingle, singl_producer_single_consumer) {
  LOGGER_TIME_CODE({ spsc<ulog::BipBuffer<uint32_t>>(1 << 4); });
  LOGGER_TIME_CODE({ spsc<ulog::BipBuffer<uint32_t>>(1 << 5); });
  LOGGER_TIME_CODE({ spsc<ulog::BipBuffer<uint32_t>>(1 << 6); });
  LOGGER_TIME_CODE({ spsc<ulog::BipBuffer<uint32_t>>(1 << 7); });
  LOGGER_TIME_CODE({ spsc<ulog::BipBuffer<uint32_t>>(1 << 8); });
  LOGGER_TIME_CODE({ spsc<ulog::BipBuffer<uint32_t>>(1 << 9); });
  LOGGER_TIME_CODE({ spsc<ulog::BipBuffer<uint32_t>>(1 << 10); });

  LOGGER_TIME_CODE({ spsc<ulog::BipBuffer2<uint32_t>>(1 << 4); });
  LOGGER_TIME_CODE({ spsc<ulog::BipBuffer2<uint32_t>>(1 << 5); });
  LOGGER_TIME_CODE({ spsc<ulog::BipBuffer2<uint32_t>>(1 << 6); });
  LOGGER_TIME_CODE({ spsc<ulog::BipBuffer2<uint32_t>>(1 << 7); });
  LOGGER_TIME_CODE({ spsc<ulog::BipBuffer2<uint32_t>>(1 << 8); });
  LOGGER_TIME_CODE({ spsc<ulog::BipBuffer2<uint32_t>>(1 << 9); });
  LOGGER_TIME_CODE({ spsc<ulog::BipBuffer2<uint32_t>>(1 << 10); });
}
