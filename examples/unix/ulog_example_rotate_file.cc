#include <unistd.h>

#include <cstdio>
#include <ctime>
#include <vector>

#include "ulog/error.h"
#include "ulog/file/sink_async_wrapper.h"
#include "ulog/file/sink_limit_size_file.h"
#include "ulog/file/sink_rotating_file.h"
#include "ulog/queue/mpsc_ring.h"
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
  std::unique_ptr<ulog::file::FileWriterBase> file_writer = std::make_unique<ulog::file::FileWriter>();
  std::unique_ptr<ulog::file::SinkBase> rotating_file = std::make_unique<ulog::file::SinkRotatingFile>(
      std::move(file_writer), "/tmp/ulog/test.txt", 100 * 1024, 5, true, ulog::file::kRename, [] {
        std::string file_head = "This is ulog lib file head.\n";
        return std::vector(file_head.begin(), file_head.end());
      });

  const auto [basename, ext] = ulog::file::SplitByExtension("/tmp/ulog/test.txt");
  std::unique_ptr<ulog::file::SinkBase> limit_size_file = std::make_unique<ulog::file::SinkLimitSizeFile>(
      std::make_unique<ulog::file::FileWriter>(), basename + "-head" + ext, 10 * 1024);
  ulog::file::SinkAsyncWrapper<ulog::mpsc::Mq> async_rotate(65536 * 2, std::chrono::seconds{1},
                                                            std::move(rotating_file), std::move(limit_size_file));

  // Initial logger
  logger_format_enable(ULOG_GLOBAL, ULOG_F_NUMBER);
  logger_set_user_data(ULOG_GLOBAL, &async_rotate);
  logger_set_output_callback(ULOG_GLOBAL, [](void *user_data, const char *str) {
    printf("%s", str);
    auto &async = *static_cast<decltype(&async_rotate)>(user_data);
    int len = strlen(str);
    return async.SinkIt(str, len) ? len : 0;
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
