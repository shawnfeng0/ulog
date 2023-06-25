#include <unistd.h>

#include <cstdio>
#include <ctime>

#include "ulog/helper/queue/fifo_power_of_two.h"
#include "ulog/ulog.h"

void *ulog_asyn_thread(void *arg) {
  auto &fifo = *(ulog::FifoPowerOfTwo *)(arg);

  char str[1024];

  size_t empty_times = 0;
  while (empty_times < 2) {
    auto len = fifo.OutputWaitIfEmpty(str, sizeof(str) - 1, 100);
    if (len > 0) {
      str[len] = '\0';
      printf("%s", str);
      empty_times = 0;
    } else {
      ++empty_times;
    }
  }

  printf("fifo.num_dropped():%zu, fifo.peak():%zu, fifo.size():%zu",
         fifo.num_dropped(), fifo.peak(), fifo.size());
  return nullptr;
}

int main() {
  auto &fifo = *new ulog::FifoPowerOfTwo{32768};

  // Initial logger
  logger_set_user_data(ULOG_GLOBAL, &fifo);
  logger_set_output_callback(ULOG_GLOBAL, [](void *user_data, const char *str) {
    auto &fifo = *(ulog::FifoPowerOfTwo *)(user_data);
    return (int)fifo.InputPacketOrDrop(str, strlen(str));
  });
  logger_set_flush_callback(ULOG_GLOBAL, [](void *user_data) {
    auto &fifo = *(ulog::FifoPowerOfTwo *)(user_data);
    fifo.Flush();
  });

  pthread_t tid;
  pthread_create(&tid, nullptr, ulog_asyn_thread, &fifo);

  uint32_t num_threads = 20;
  while (num_threads--) {
    pthread_t tid_output;
    pthread_create(
        &tid_output, nullptr,
        [](void *unused) -> void * {
          (void)unused;
          double pi = 3.14159265;
          LOGGER_DEBUG("PI = %.3f", pi);
          LOGGER_RAW("PI = %.3f\r\n", pi);

          // Output debugging expression
          LOGGER_TOKEN(pi);
          LOGGER_TOKEN(50 * pi / 180);
          LOGGER_TOKEN(&pi);  // print address of pi

          const char *text = "Ulog is a micro log library.";
          LOGGER_TOKEN(text);

          // Hex dump
          LOGGER_HEX_DUMP(text, 45, 16);

          // Output multiple tokens to one line
          time_t now = 1577259816;
          struct tm lt = *localtime(&now);

          LOGGER_MULTI_TOKEN(lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
          LOGGER_MULTI_TOKEN(lt.tm_wday, lt.tm_hour, lt.tm_min, lt.tm_sec);

          return nullptr;
        },
        nullptr);
  }

  pthread_exit(nullptr);
}
