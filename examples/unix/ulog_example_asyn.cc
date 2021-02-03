#include <unistd.h>

#include <cstdio>
#include <ctime>

#include "ulog/helper/fifo_power_of_two.h"
#include "ulog/ulog.h"

static uint64_t get_time_us() {
  struct timespec tp = {0, 0};
  clock_gettime(CLOCK_REALTIME, &tp);
  return static_cast<uint64_t>(tp.tv_sec) * 1000 * 1000 + tp.tv_nsec / 1000;
}

void *ulog_asyn_thread(void *arg) {
  auto &fifo = *(ulog::FifoPowerOfTwo *)(arg);

  uint64_t start_us = get_time_us();
  uint64_t time_out_us = start_us + 10 * 1000 * 1000;
  char str[500];
  while (get_time_us() < time_out_us) {
    memset(str, '\0', sizeof(str));
    if (fifo.OutWaitIfEmpty(str, sizeof(str) - 1, 1000) > 0) printf("%s", str);
  }

  printf("fifo.num_dropped():%zu, fifo.peak():%zu, fifo.size():%zu",
         fifo.num_dropped(), fifo.peak(), fifo.size());
  return nullptr;
}

int main() {
  auto &fifo = *new ulog::FifoPowerOfTwo{32768};

  // Initial logger
  logger_set_output_callback(
      ULOG_GLOBAL, &fifo, [](void *private_data, const char *str) {
        auto &fifo = *(ulog::FifoPowerOfTwo *)(private_data);
        return (int)fifo.InPacket(str, strlen(str));
      });

  pthread_t tid;
  pthread_create(&tid, nullptr, ulog_asyn_thread, &fifo);

  uint32_t num_threads = 20;
  while (num_threads--) {
    pthread_t tid_output;
    pthread_create(
        &tid_output, nullptr,
        [](void *arg) -> void * {
          double pi = 3.14159265;
          LOGGER_DEBUG("PI = %.3f", pi);
          LOGGER_RAW("PI = %.3f\r\n", pi);

          // Output debugging expression
          LOGGER_TOKEN(pi);
          LOGGER_TOKEN(50 * pi / 180);
          LOGGER_TOKEN(&pi);  // print address of pi

          char *text = (char *)"Ulog is a micro log library.";
          LOGGER_TOKEN((char *)text);

          // Hex dump
          LOGGER_HEX_DUMP(text, 45, 16);

          LOGGER_TIME_CODE(int a = 0;);

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
