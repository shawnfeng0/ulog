//
// Created by shawnfeng on 23-3-31.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <random>
#include <thread>

#include "ulog/queue/spsc_ring.h"
#include "ulog/ulog.h"

template <typename T>
static void spsc(const uint32_t buffer_size, const uint64_t limit) {
  auto buffer = T::Create(buffer_size);

  std::thread write_thread{[&] {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(1, std::max<uint32_t>(buffer_size / 100, 2));
    uint64_t write_count = 0;
    while (write_count < limit) {
      size_t size = dis(gen);

      decltype(buffer->Reserve(size)) data;
      while ((data = buffer->Reserve(size)) == nullptr) {
        std::this_thread::yield();
      }
      for (size_t i = 0; i < size; ++i) data[i] = write_count++;
      buffer->Commit(data, size);
    }
  }};

  std::thread read_thread{[&] {
    uint64_t read_count = 0;
    while (read_count < limit) {
      auto data = buffer->TryRead();

      if (!data) {
        std::this_thread::yield();
        continue;
      }

      while (const auto packet = data.next()) {
        for (size_t i = 0; i < packet.size; ++i) {
          ASSERT_EQ(packet.data[i], read_count++);
        }
      }
      buffer->Release(data);
    }
  }};

  write_thread.join();
  read_thread.join();
  printf("Finished test: buffer_size: %u, limit: %" PRIu64 "\n", buffer_size, limit);
}

TEST(BipBufferTestSingle, singl_producer_single_consumer) {
  LOGGER_TIME_CODE({ spsc<ulog::spsc::Mq<uint32_t>>(1 << 4, 1024 * 1024); });
  LOGGER_TIME_CODE({ spsc<ulog::spsc::Mq<uint32_t>>(1 << 6, 1024 * 1024); });
  LOGGER_TIME_CODE({ spsc<ulog::spsc::Mq<uint32_t>>(1 << 8, 1024 * 1024); });
  LOGGER_TIME_CODE({ spsc<ulog::spsc::Mq<uint32_t>>(1 << 10, 1024 * 1024); });
  LOGGER_TIME_CODE({ spsc<ulog::spsc::Mq<uint32_t>>(1 << 12, 1024 * 1024); });
  LOGGER_TIME_CODE({ spsc<ulog::spsc::Mq<uint32_t>>(1 << 14, 1024 * 1024); });
  LOGGER_TIME_CODE({ spsc<ulog::spsc::Mq<uint32_t>>(1 << 16, 1024 * 1024); });
}
