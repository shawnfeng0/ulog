#include <unistd.h>

#include <cstdio>
#include <ctime>
#include <vector>

#include "ulog/helper/async_rotating_file.h"
#include "ulog/helper/fifo_power_of_two.h"
#include "ulog/helper/rotating_file.h"
#include "ulog/ulog.h"

static void OutputFunc() {
  int i = 10;
  LOGGER_TIME_CODE(while (i--) {
    double pi = 3.14159265;
    LOGGER_DEBUG("PI = %.3f", pi);

    LOGGER_ERROR("Error log test");

    // Output debugging expression
    LOGGER_TOKEN(pi);
    LOGGER_TOKEN(50 * pi / 180);
    LOGGER_TOKEN(&pi);  // print address of pi

    char *text = (char *)"Ulog is a micro log library.";
    LOGGER_TOKEN((char *)text);

    // Hex dump
    LOGGER_HEX_DUMP(text, 45, 16);

    // Output multiple tokens to one line
    time_t now = 1577259816;
    struct tm lt = *localtime(&now);

    LOGGER_MULTI_TOKEN(lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
    LOGGER_MULTI_TOKEN(lt.tm_wday, lt.tm_hour, lt.tm_min, lt.tm_sec);
  });
}

int main(int argc, char *argv[]) {
  auto &async_rotate = *new ulog::AsyncRotatingFile(
      65536 * 2, "/tmp/ulog/test.txt", 100 * 1024, 5, 1, true);

  // Initial logger
  logger_set_user_data(ULOG_GLOBAL, &async_rotate);
  logger_set_output_callback(ULOG_GLOBAL, [](void *user_data, const char *str) {
    auto &async = *(ulog::AsyncRotatingFile *)(user_data);
    return (int)async.InPacket(str, strlen(str));
  });
  logger_set_flush_callback(ULOG_GLOBAL, [](void *user_data) {
    auto &async = *(ulog::AsyncRotatingFile *)(user_data);
    async.Flush();
  });

  // Create some output thread
  std::vector<std::thread> threads;
  uint32_t num_threads = 10;
  while (num_threads--) threads.emplace_back(OutputFunc);

  for (auto &thread : threads) thread.join();

  // Wait flush
  while (!async_rotate.is_idle()) usleep(1000);

  printf("fifo.num_dropped():%zu, fifo.peak():%zu, fifo.size():%zu\n",
         async_rotate.fifo_num_dropped(), async_rotate.fifo_peak(),
         async_rotate.fifo_size());

  delete &async_rotate;

  pthread_exit(nullptr);
}
