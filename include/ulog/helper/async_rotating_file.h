//
// Created by fs on 2020-05-26.
//

#ifndef ULOG_INCLUDE_ULOG_HELPER_ASYNC_ROTATING_FILE_H_
#define ULOG_INCLUDE_ULOG_HELPER_ASYNC_ROTATING_FILE_H_

#include <string>
#include <utility>

#include "ulog/helper/fifo_power_of_two.h"
#include "ulog/helper/rotating_file.h"

namespace ulog {

class AsyncRotatingFile {
 public:
  AsyncRotatingFile(size_t fifo_size, std::string filename,
                    std::size_t max_file_size, std::size_t max_files,
                    bool should_print = false)
      : should_print_(should_print),
        fifo_(fifo_size),
        rotating_file_(std::move(filename), max_file_size, max_files),
        async_thread_([&] {
          uint8_t data[1024];
          while (!should_exit_) {
            int len = fifo_.OutWaitIfEmpty(data, sizeof(data) - 1, 1000);
            if (len > 0) {
              rotating_file_.SinkIt(data, len);
              if (should_print_) {
                data[len] = '\0';  // Generate C string
                printf("%s", data);
              }
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

  size_t fifo_size() { return fifo_.size(); }
  size_t fifo_num_dropped() { return fifo_.num_dropped(); }
  size_t fifo_peak() { return fifo_.peak(); }
  bool is_idle() { return fifo_.empty(); }

 private:
  FifoPowerOfTwo fifo_;
  RotatingFile rotating_file_;
  std::thread async_thread_;
  const bool should_print_;

  bool should_exit_ = false;
};

}  // namespace ulog

#endif  // ULOG_INCLUDE_ULOG_HELPER_ASYNC_ROTATING_FILE_H_
