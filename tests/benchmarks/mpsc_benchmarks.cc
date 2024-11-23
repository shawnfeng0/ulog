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
      constexpr char data_source[] =
          "asdfqwerasdfqwerasdfqwerasdfqweasdfqwerasdfqwerasdfqweasdfqwerasdfqwerasdfqweasdfqwerasdfqwerasdfqweasdfqwer"
          "as"
          "dfqwerasdfqweasdfqwerasdfqwerasdfqweasdfqwerasdfqwerasdfqweasdfqwerasdfqwerasdfqweasdfqwerasdfqwerasdfqwerrr"
          "rr"
          "sdfeeraerasdfqwersdfqwerrerasdfqwer";
      memcpy(data, data_source, std::min(sizeof(data_source), size));
      producer.Commit();
    }
  };

  std::vector<std::thread> write_thread;
  for (size_t i = 0; i < max_write_thread; ++i) write_thread.emplace_back(write_entry);

  std::thread{[=] {
    ulog::umq::Consumer<buffer_size> consumer(buffer);
    size_t total_packet = 0;
    size_t packet_count;
    uint8_t data_recv[100 * 1024];
    while (ulog::umq::PacketPtr ptr = consumer.ReadOrWait(&packet_count, 10)) {
      total_packet += packet_count;
      for (size_t i = 0; i < packet_count; i++) {
        memcpy(data_recv, ptr->data, ptr->size());
        ptr = ptr.next();
      }
      consumer.ReleasePacket();
    }
    LOGGER_MULTI_TOKEN(buffer_size, total_packet);
  }}.detach();

  for (auto& t : write_thread) t.join();
}

template <int buffer_size>
static void fifo_mpsc(const size_t max_write_thread, const size_t publish_count) {
  auto fifo = new ulog::FifoPowerOfTwo(buffer_size, 1);

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
      fifo->InputWaitIfFull(data_source, size, 100);
    }
  };

  std::vector<std::thread> write_thread;
  for (size_t i = 0; i < max_write_thread; ++i) write_thread.emplace_back(write_entry);

  std::thread{[=] {
    uint64_t total_num = 0;
    uint8_t data_recv[100 * 1024];
    while (const auto size = fifo->OutputWaitIfEmpty(data_recv, sizeof(data_recv), 10)) {
      total_num += size;
    }
    LOGGER_MULTI_TOKEN(buffer_size, total_num);
  }}.detach();

  for (auto& t : write_thread) t.join();
}

int main() {
  logger_format_disable(ulog_global_logger,
                        ULOG_F_FUNCTION | ULOG_F_TIME | ULOG_F_PROCESS_ID | ULOG_F_LEVEL | ULOG_F_FILE_LINE);
  logger_set_output_level(ulog_global_logger, ULOG_LEVEL_INFO);
  // LOGGER_TIME_CODE({ mpsc<64 * 1024>(200, 10 * 1024); });
  for (int thread_count = 1; thread_count <= 200;
       thread_count = thread_count < 10 ? thread_count <<= 1 : thread_count + 10) {
    auto mpsc_time = LOGGER_TIME_CODE({ mpsc<64 * 1024>(thread_count, 10 * 1024); });
    auto fifo_time = LOGGER_TIME_CODE({ fifo_mpsc<64 * 1024>(thread_count, 10 * 1024); });
    LOGGER_INFO("%d %lu %lu", thread_count, mpsc_time, fifo_time);
  }
  pthread_exit(nullptr);
}
