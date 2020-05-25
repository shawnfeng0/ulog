#include <unistd.h>

#include <cstdio>
#include <ctime>
#include <iostream>

#include "ulog/helper/async_rotating_file.h"
#include "ulog/helper/fifo_power_of_two.h"
#include "ulog/helper/rotating_file.h"
#include "ulog/ulog.h"

static void OutputFunc() {
  int i = 10;
  LOG_TIME_CODE(while (i--) {
    double pi = 3.14159265;
    LOG_DEBUG("PI = %.3f", pi);

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
  });
}

int main(int argc, char *argv[]) {
  auto &async_rotate = *new ulog::AsyncRotatingFile(
      65536, "/tmp/ulog_test/test.txt", 100 * 1024, 5, true);

  // Initial logger
  logger_init(&async_rotate, [](void *private_data_unused, const char *str) {
    auto &async = *(ulog::AsyncRotatingFile *)(private_data_unused);
    return (int)async.InPacket(str, strlen(str));
  });

  uint32_t num_threads = 2;
  while (num_threads--) {
    std::thread output_thread(OutputFunc);
    output_thread.detach();
  }

  sleep(1);

  printf("fifo.num_dropped():%zu, fifo.peak():%zu, fifo.size():%zu\n",
         async_rotate.fifo_num_dropped(), async_rotate.fifo_peak(),
         async_rotate.fifo_size());

  delete &async_rotate;

  pthread_exit(nullptr);
}
