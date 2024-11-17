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
static void spsc(const size_t max_write_thread = 4) {
  const uint64_t limit = buffer_size * 8192;
  ulog::umq::Umq<buffer_size> buffer;
  std::atomic_uint64_t write_count{0};

  auto write_entry = [&] {
    ulog::umq::Producer<buffer_size> producer(&buffer);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(1, std::max<uint32_t>(buffer_size / 100, 2));
    while (true) {
      if (write_count > limit) break;

      size_t size = dis(gen);
      const auto data = static_cast<uint8_t*>(producer.TryReserve(size));

      if (data == nullptr) {
        std::this_thread::yield();
        continue;
      }

      write_count += size;
      for (size_t i = 0; i < size; ++i) data[i] = reinterpret_cast<uint64_t>(data - i) & 0xff;
      producer.Commit();
    }
  };

  std::vector<std::thread> write_thread;
  for (size_t i = 0; i < max_write_thread; ++i) write_thread.emplace_back(write_entry);

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

      read_count += size;
      for (size_t i = 0; i < size; ++i) {
        ASSERT_EQ(data[i], reinterpret_cast<uint64_t>(data - i) & 0xff);
      }
      consumer.ReleasePacket();
    }
  }};

  for (auto& t : write_thread) t.join();
  read_thread.join();
  printf("Finished test: buffer_size: %u, limit: %" PRIu64 "\n", buffer_size, limit);
}

TEST(MpscRingTest, singl_producer_single_consumer) {
  LOGGER_TIME_CODE({ spsc<1 << 5>(16); });
  LOGGER_TIME_CODE({ spsc<1 << 6>(16); });
  LOGGER_TIME_CODE({ spsc<1 << 7>(16); });
  LOGGER_TIME_CODE({ spsc<1 << 8>(16); });
  LOGGER_TIME_CODE({ spsc<1 << 9>(16); });
  LOGGER_TIME_CODE({ spsc<1 << 10>(16); });
}
