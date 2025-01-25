#include <unistd.h>

#include <cstdio>
#include <ctime>
#include <vector>

#include "ulog/error.h"
#include "ulog/file/async_rotating_file.h"
#include "ulog/queue/mpsc_ring.h"
#include "ulog/queue/spsc_ring.h"
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

    const char *text = "Ulog is a micro log library.";
    LOGGER_TOKEN(text);

    // Hex dump
    LOGGER_HEX_DUMP(text, 45, 16);

    // Output multiple tokens to one line
    time_t now = 1577259816;
    struct tm lt = *localtime(&now);

    LOGGER_MULTI_TOKEN(lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
    LOGGER_MULTI_TOKEN(lt.tm_wday, lt.tm_hour, lt.tm_min, lt.tm_sec);
  });
}

int main() {
  std::unique_ptr<ulog::WriterInterface> file_writer = std::make_unique<ulog::FileLimitWriter>(100 * 1024);
  ulog::file::AsyncRotatingFile<ulog::mpsc::Mq> async_rotate(std::move(file_writer), 65536 * 2, "/tmp/ulog/test.txt", 5,
                                                             true, std::chrono::seconds{1}, ulog::file::kRename);

  // Initial logger
  logger_set_user_data(ULOG_GLOBAL, &async_rotate);
  logger_set_output_callback(ULOG_GLOBAL, [](void *user_data, const char *str) {
    printf("%s", str);
    auto &async = *static_cast<decltype(&async_rotate)>(user_data);
    return static_cast<int>(async.InPacket(str, strlen(str)));
  });
  logger_set_flush_callback(ULOG_GLOBAL, [](void *user_data) {
    auto &async = *static_cast<decltype(&async_rotate)>(user_data);
    const auto status = async.Flush();
    if (!status) {
      ULOG_ERROR("Failed to flush file: %s", status.ToString().c_str());
    }
  });

  // Create some output thread
  std::vector<std::thread> threads;
  uint32_t num_threads = 10;
  while (num_threads--) threads.emplace_back(OutputFunc);
  for (auto &thread : threads) thread.join();

  return 0;
}
