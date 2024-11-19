//
// Created by shawnfeng on 23-3-31.
//

#include <gtest/gtest.h>

#include <random>
#include <thread>

#include "ulog/helper/queue/fifo_power_of_two.h"
#include "ulog/helper/queue/mpsc_ring.h"
#include "ulog/ulog.h"

template <int buffer_size>
static void mpsc(const size_t max_write_thread, const size_t publish_count) {
  auto* buffer = new ulog::umq::Umq<buffer_size>;

  auto write_entry = [=] {
    ulog::umq::Producer<buffer_size> producer(buffer);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(8, 256);

    uint64_t total_num = 0;
    while (total_num++ < publish_count) {
      size_t size = dis(gen);
      const auto data = static_cast<uint8_t*>(producer.ReserveOrWait(size, 100));
      if (data == nullptr) {
        continue;
      }
      producer.Commit();
    }
  };

  std::vector<std::thread> write_thread;
  for (size_t i = 0; i < max_write_thread; ++i) write_thread.emplace_back(write_entry);

  std::thread{[=] {
    ulog::umq::Consumer<buffer_size> consumer(buffer);
    uint64_t total_num = 0;
    uint32_t size;
    while (consumer.ReadOrWait(&size, 500)) {
      total_num += 1;
      consumer.ReleasePacket();
    }
    LOGGER_MULTI_TOKEN(buffer_size, total_num);
  }}.detach();

  for (auto& t : write_thread) t.join();
}

template <int buffer_size>
static void fifo_mpsc(const size_t max_write_thread, const size_t publish_count) {
  auto buffer = new ulog::FifoPowerOfTwo(buffer_size, 1);

  auto write_entry = [=] {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(8, 256);

    constexpr uint8_t data_source[] =
        "asdfqwerasdfqwerasdfqwerasdfqweasdfqwerasdfqwerasdfqweasdfqwerasdfqwerasdfqweasdfqwerasdfqwerasdfqweasdfqweras"
        "dfqwerasdfqweasdfqwerasdfqwerasdfqweasdfqwerasdfqwerasdfqweasdfqwerasdfqwerasdfqweasdfqwerasdfqwerasdfqwerrrrr"
        "sdfeeraerasdfqwersdfqwerrerasdfqwer";

    uint64_t total_num = 0;
    while (total_num++ < publish_count) {
      size_t size = dis(gen);
      buffer->InputWaitUntil(data_source, size, 100, [&] { return buffer->unused() > size; });
    }
  };

  std::vector<std::thread> write_thread;
  for (size_t i = 0; i < max_write_thread; ++i) write_thread.emplace_back(write_entry);

  std::thread{[=] {
    uint64_t total_num = 0;
    uint8_t data_recv[10 * 1024];
    while (buffer->OutputWaitIfEmpty(data_recv, sizeof(data_recv), 500)) {
      total_num += 1;
    }
    LOGGER_MULTI_TOKEN(buffer_size, total_num);
  }}.detach();

  for (auto& t : write_thread) t.join();
}

int main() {
  // LOGGER_TIME_CODE({ mpsc<64 * 1024>(128, 10 * 1024); });
  LOGGER_TIME_CODE({ fifo_mpsc<64 * 1024>(128, 10 * 1024); });
  pthread_exit(nullptr);
}
