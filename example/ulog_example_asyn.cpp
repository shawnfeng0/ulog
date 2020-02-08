#include "ulog/helper/fifo_power_of_two.hpp"
#include "ulog/ulog.h"

#include <stdio.h>
#include <time.h>
#include <unistd.h>

static char fifo_buffer[260];
static FifoPowerOfTwo fifo(fifo_buffer, sizeof(fifo_buffer));

static uint64_t get_time_us() {
  struct timespec tp = {0, 0};
  clock_gettime(CLOCK_REALTIME, &tp);
  return static_cast<uint64_t>(tp.tv_sec * 1000 * 1000 + tp.tv_nsec / 1000);
}

static int put_str(const char *str) {
  return (int)fifo.InPacket(str, strlen(str));
}

void *ulog_asyn_thread(void *) {
  uint64_t start_ms = get_time_us() / 1000;
  uint64_t time_out_ms = 10 * 1000;
  char str[500];
  while ((get_time_us() / 1000 - start_ms) < time_out_ms) {
    memset(str, '\0', sizeof(str));
    fifo.Out(str, sizeof(str));
    printf("%s", str);
  }
  return nullptr;
}

int main() {
  // Initial logger
  logger_set_time_callback(get_time_us);
  logger_init(put_str);

  LOG_MULTI_TOKEN(fifo.GetNumDropped(), fifo.GetPeak(), fifo.GetSize());

  pthread_t tid;
  pthread_create(&tid, nullptr, ulog_asyn_thread, nullptr);

  double pi = 3.14159265;
  for (int i = 0; i < 100; i++) {
    // Different log levels
    LOG_TRACE("PI = %.3f", pi);
    LOG_DEBUG("PI = %.3f", pi);
    LOG_INFO("PI = %.3f", pi);
    LOG_WARN("PI = %.3f", pi);
    LOG_ERROR("PI = %.3f", pi);
    LOG_FATAL("PI = %.3f", pi);
    LOG_RAW("PI = %.3f\r\n", pi);

    // Output debugging expression
    LOG_TOKEN(pi);
    LOG_TOKEN(50 * pi / 180);
    LOG_TOKEN(&pi); // print address of pi

    char *text = (char *)"Ulog is a micro log library.";
    LOG_TOKEN((char *)text);

    // Hex dump
    LOG_HEX_DUMP(text, 45, 16);

    // Output multiple tokens to one line
    time_t now = 1577259816;
    struct tm lt = *localtime(&now);

    LOG_MULTI_TOKEN(lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
    LOG_MULTI_TOKEN(lt.tm_wday, lt.tm_hour, lt.tm_min, lt.tm_sec);

    // Output execution time of some statements
    LOG_TIME_CODE(

        uint32_t n = 1000 * 1000; while (n--);

    );
  }

  LOG_MULTI_TOKEN(fifo.GetNumDropped(), fifo.GetPeak(), fifo.GetSize());

  LOG_TOKEN((void *) new char [0]);

  pthread_exit(nullptr);
}
