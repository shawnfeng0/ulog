#include <cstdio>
#include <ctime>
#include <unistd.h>

#include "ulog/helper/fifo_power_of_two.hpp"
#include "ulog/ulog.h"

static char fifo_buffer[65536];
static FifoPowerOfTwo fifo(fifo_buffer, sizeof(fifo_buffer));

static uint64_t get_time_us() {
  struct timespec tp = {0, 0};
  clock_gettime(CLOCK_REALTIME, &tp);
  return static_cast<uint64_t>(tp.tv_sec) * 1000 * 1000 + tp.tv_nsec / 1000;
}

void *ulog_asyn_thread(void *) {
  uint64_t start_us = get_time_us();
  uint64_t time_out_us = start_us + 10 * 1000 * 1000;
  char str[500];
  while (get_time_us() < time_out_us) {
    memset(str, '\0', sizeof(str));
    if (fifo.OutWaitIfEmpty(str, sizeof(str) - 1, 1000) > 0) printf("%s", str);
  }

  printf("fifo.num_dropped():%d, fifo.peak():%d, fifo.size():%d",
         fifo.num_dropped(), fifo.peak(), fifo.size());
  return nullptr;
}

int main() {
  // Initial logger
  logger_set_time_callback(get_time_us);
  logger_init(
      [](const char *str) { return (int)fifo.InPacket(str, strlen(str)); });

  LOG_MULTI_TOKEN(fifo.num_dropped(), fifo.peak(), fifo.size());

  pthread_t tid;
  pthread_create(&tid, nullptr, ulog_asyn_thread, nullptr);

  uint32_t num_threads = 100;
  while (num_threads--) {
    pthread_t tid_output;
    pthread_create(
        &tid_output, nullptr,
        [](void *arg) -> void * {
          double pi = 3.14159265;
          LOG_DEBUG("PI = %.3f", pi);
          LOG_RAW("PI = %.3f\r\n", pi);

          // Output debugging expression
          LOG_TOKEN(pi);
          LOG_TOKEN(50 * pi / 180);
          LOG_TOKEN(&pi);  // print address of pi

          char *text = (char *)"Ulog is a micro log library.";
          LOG_TOKEN((char *)text);

          // Hex dump
          LOG_HEX_DUMP(text, 45, 16);

          // Output multiple tokens to one line
          time_t now = 1577259816;
          struct tm lt = *localtime(&now);

          LOG_MULTI_TOKEN(lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
          LOG_MULTI_TOKEN(lt.tm_wday, lt.tm_hour, lt.tm_min, lt.tm_sec);

          return nullptr;
        },
        nullptr);
  }

  pthread_exit(nullptr);
}
