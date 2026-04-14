//
// Created by shawnfeng on 23-3-31.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <random>
#include <thread>

#include "ulog/queue/fifo_power_of_two.h"
#include "ulog/queue/mpsc_ring.h"
#include "ulog/ulog.h"

// Returns ANSI color string for ratio value
#include <string>
static std::string get_ratio_color(float ratio, float min = -0.5f, float max = 0.5f) {
  float clamped = std::clamp(ratio, min, max);
  int target_r = clamped < 0 ? 255 : 0;
  int target_g = clamped < 0 ? 0 : 255;
  int target_b = 0;
  float t = (max != min) ? fabsf(clamped) / (max - min) * 2.0f : 0.0f;
  if (t > 1.0f) t = 1.0f;
  int r = (int)(255 * (1.0f - t) + target_r * t);
  int g = (int)(255 * (1.0f - t) + target_g * t);
  int b = (int)(255 * (1.0f - t) + target_b * t);
  char buf[32];
  snprintf(buf, sizeof(buf), "\033[38;2;%d;%d;%dm", r, g, b);
  return std::string(buf);
}

static void umq_mpsc(const size_t buffer_size, const size_t max_write_thread, const size_t publish_count,
                     const size_t max_read_thread) {
  const auto umq = ulog::mpsc::Mq::Create(buffer_size);

  auto write_entry = [=] {
    ulog::mpsc::Mq::Producer producer(umq);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(8, 256);

    uint64_t total_num = 0;
    while (total_num++ < publish_count) {
      size_t size = dis(gen);
      const auto data = static_cast<uint8_t*>(producer.ReserveOrWaitFor(size, std::chrono::milliseconds(100)));
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
      producer.Commit(data, size);
    }
    producer.Flush();
  };

  std::vector<std::thread> write_thread;
  for (size_t i = 0; i < max_write_thread; ++i) write_thread.emplace_back(write_entry);

  auto read_entry = [=] {
    ulog::mpsc::Mq::Consumer consumer(umq);
    size_t total_packet = 0;
    while (ulog::mpsc::DataPacket data = consumer.ReadOrWait(std::chrono::milliseconds(10))) {
      total_packet += data.remain();
      while (const auto packet = data.next()) {
        uint8_t data_recv[100 * 1024];
        memcpy(data_recv, packet.data, packet.size);
      }
      consumer.Release(data);
    }
    LOGGER_MULTI_TOKEN(buffer_size, total_packet);
  };
  std::vector<std::thread> read_thread;
  for (size_t i = 0; i < max_read_thread; ++i) read_thread.emplace_back(read_entry);

  for (auto& t : write_thread) t.join();
  for (auto& t : read_thread) t.join();
}

static void fifo_mpsc(const size_t buffer_size, const size_t max_write_thread, const size_t publish_count,
                      const size_t max_read_thread) {
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

  auto read_entry = [=] {
    uint64_t total_num = 0;
    uint8_t data_recv[100 * 1024];
    while (const auto size = fifo->OutputWaitIfEmpty(data_recv, sizeof(data_recv), 10)) {
      total_num += size;
    }
    LOGGER_MULTI_TOKEN(buffer_size, total_num);
  };
  std::vector<std::thread> read_thread;
  for (size_t i = 0; i < max_read_thread; ++i) read_thread.emplace_back(read_entry);

  for (auto& t : write_thread) t.join();
  for (auto& t : read_thread) t.join();
}

int main() {
  logger_format_disable(ulog_global_logger,
                        ULOG_F_FUNCTION | ULOG_F_TIME | ULOG_F_PROCESS_ID | ULOG_F_LEVEL | ULOG_F_FILE_LINE);
  logger_set_output_level(ulog_global_logger, ULOG_LEVEL_INFO);

  auto mpsc_test = [](const size_t buffer_size) {
    constexpr size_t publish_count = 10 * 1024;
    LOGGER_INFO("Algorithm buffer size: %lu", buffer_size);
    LOGGER_INFO("Number of packet published per thread: %lu", publish_count);
    LOGGER_INFO("Each packet has 8 ~ 256 bytes.");
    LOGGER_INFO("%16s %16s %16s %16s", "write_thread", "umq", "kfifo+mutex", "ratio");
    for (int thread_count = 1; thread_count <= 200;
         thread_count = thread_count < 10 ? thread_count << 1 : thread_count + 10) {
      int64_t mpsc_time = LOGGER_TIME_CODE({ umq_mpsc(buffer_size, thread_count, publish_count, 1); });
      int64_t fifo_time = LOGGER_TIME_CODE({ fifo_mpsc(buffer_size, thread_count, publish_count, 1); });
      float ratio = (float)(fifo_time - mpsc_time) / fifo_time;
      const std::string color_start = get_ratio_color(ratio);
      LOGGER_INFO("\033[0m%16d %14lums %14lums %s%16.2f", thread_count, mpsc_time, fifo_time, color_start.c_str(),
                  ratio);
    }
  };

  mpsc_test(64 * 1024);
  mpsc_test(64 * 1024 * 8);

  pthread_exit(nullptr);
}
