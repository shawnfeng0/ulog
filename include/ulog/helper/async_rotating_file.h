//
// Created by fs on 2020-05-26.
//

#ifndef ULOG_INCLUDE_ULOG_HELPER_ASYNC_ROTATING_FILE_H_
#define ULOG_INCLUDE_ULOG_HELPER_ASYNC_ROTATING_FILE_H_

#include <ctime>
#include <string>
#include <utility>

#include "ulog/helper/fifo_power_of_two.h"
#include "ulog/helper/rotating_file.h"

namespace ulog {

class AsyncRotatingFile {
 public:
  /**
   * Build a logger that asynchronously outputs data to a file
   * @param fifo_size Asynchronous first-in first-out buffer size
   * @param filename File name for storing data
   * @param max_file_size Maximum size of a single file
   * @param max_files Number of files that can be divided
   * @param max_flush_period_sec Maximum file flush period (some file systems
   * and platforms only refresh once every 60s by default, which is too slow)
   * @param should_print Can be printed using printf function
   */
  AsyncRotatingFile(size_t fifo_size, std::string filename,
                    std::size_t max_file_size, std::size_t max_files,
                    std::time_t max_flush_period_sec = 0,
                    bool should_print = false)
      : should_print_(should_print),
        fifo_(fifo_size),
        rotating_file_(std::move(filename), max_file_size, max_files),
        flush_period_sec_(max_flush_period_sec),
        async_thread_([&] {
          uint8_t data[1024];
          std::time_t last_flush_time = std::time(nullptr);
          while (!should_exit_) {
            int len = fifo_.OutWaitIfEmpty(data, sizeof(data) - 1, 1000);
            if (len > 0) {
              rotating_file_.SinkIt(data, len);
              if (should_print_) {
                data[len] = '\0';  // Generate C string
                printf("%s", data);
              }
            }

            // Flush
            std::time_t cur_time = std::time(nullptr);
            if (cur_time - last_flush_time >= flush_period_sec_) {
              last_flush_time = cur_time;
              rotating_file_.Flush();
            }
          }
        }) {}

  AsyncRotatingFile(const AsyncRotatingFile &) = delete;
  AsyncRotatingFile &operator=(const AsyncRotatingFile &) = delete;

  ~AsyncRotatingFile() {
    should_exit_ = true;
    async_thread_.join();
  }

  size_t InPacket(const void *buf, size_t num_elements) {
    return fifo_.InPacket(buf, num_elements);
  }

  size_t fifo_size() const { return fifo_.size(); }
  size_t fifo_num_dropped() const { return fifo_.num_dropped(); }
  size_t fifo_peak() const { return fifo_.peak(); }
  bool is_idle() const { return fifo_.empty(); }

 private:
  FifoPowerOfTwo fifo_;
  RotatingFile rotating_file_;
  std::thread async_thread_;
  const bool should_print_;
  std::time_t flush_period_sec_;

  bool should_exit_ = false;
};

}  // namespace ulog

#endif  // ULOG_INCLUDE_ULOG_HELPER_ASYNC_ROTATING_FILE_H_
