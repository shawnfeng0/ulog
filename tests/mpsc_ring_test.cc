//
// Created by shawnfeng on 23-3-31.
//

#include "ulog/helper/queue/mpsc_ring.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <random>
#include <thread>

#include "ulog/ulog.h"

template <int buffer_size>
static void spsc() {
  const uint64_t limit = buffer_size * 8192;
  ulog::umq::Umq<buffer_size> buffer;

  std::thread write_thread{[&] {
    ulog::umq::Producer<buffer_size> producer(&buffer);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(
        1, std::max<uint32_t>(buffer_size / 10, 2));
    uint64_t write_count = 0;
    while (write_count < limit) {
      size_t size = dis(gen);
      auto data = static_cast<uint8_t*>(producer.TryReserve(size));
      if (data == nullptr) {
        std::this_thread::yield();
        continue;
      }
      for (size_t i = 0; i < size; ++i) data[i] = (write_count++) & 0xff;
      producer.Commit();
    }
  }};

  std::thread read_thread{[&] {
    ulog::umq::Consumer<buffer_size> consumer(&buffer);
    uint64_t read_count = 0;
    while (read_count < limit) {
      uint32_t size;
      const auto data = static_cast<uint8_t*>(consumer.TryReadOnePacket(&size));
      if (data == nullptr) {
        std::this_thread::yield();
        continue;
      }
      for (size_t i = 0; i < size; ++i) {
        const auto count = (read_count++) & 0xff;
        ASSERT_EQ(data[i], count);
        // if (data[i] != count) {
        //   printf("data: 0x%x, count: 0x%x\n", data[i], count);
        //   consumer.Debug();
        // }
      }
      consumer.ReleasePacket();
    }
  }};

  write_thread.join();
  read_thread.join();
  printf("Finished test: buffer_size: %u, limit: %" PRIu64 "\n", buffer_size, limit);
}

TEST(MpscRingTest, singl_producer_single_consumer) {
  LOGGER_TIME_CODE({ spsc<1 << 5>(); });
  LOGGER_TIME_CODE({ spsc<1 << 6>(); });
  LOGGER_TIME_CODE({ spsc<1 << 7>(); });
  LOGGER_TIME_CODE({ spsc<1 << 8>(); });
  LOGGER_TIME_CODE({ spsc<1 << 9>(); });
  LOGGER_TIME_CODE({ spsc<1 << 10>(); });
}
