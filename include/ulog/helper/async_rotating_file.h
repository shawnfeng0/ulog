//
// Created by fs on 2020-05-26.
//

#pragma once

#include <atomic>
#include <ctime>
#include <iostream>
#include <string>
#include <utility>

#include "ulog/helper/queue/fifo_power_of_two.h"
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
                    std::time_t max_flush_period_sec = 0)
      : fifo_(fifo_size),
        rotating_file_(std::move(filename), max_file_size, max_files),
        flush_period_sec_(max_flush_period_sec) {
    auto async_thread_function = [&]() {
      uint8_t data[2 * 1024];
      std::time_t last_flush_time = std::time(nullptr);
      while (!should_exit_) {
        auto len = fifo_.OutputWaitUntil(data, sizeof(data) - 1, 1000, [&] {
          return !fifo_.empty() || need_flush_;
        });
        if (len > 0) {
          rotating_file_.SinkIt(data, len);
        }

        // Flush
        std::time_t const cur_time = std::time(nullptr);
        if (need_flush_ || (cur_time - last_flush_time >= flush_period_sec_)) {
          need_flush_ = false;
          last_flush_time = cur_time;
          rotating_file_.Flush();
          ++flush_count_;
        }
      }
    };
    async_thread_ =
        std::unique_ptr<std::thread>{new std::thread{async_thread_function}};
  }

  AsyncRotatingFile(const AsyncRotatingFile &) = delete;
  AsyncRotatingFile &operator=(const AsyncRotatingFile &) = delete;

  ~AsyncRotatingFile() {
    should_exit_ = true;
    fifo_.InterruptOutput();
    if (async_thread_) async_thread_->join();
  }

  void Flush() {
    fifo_.Flush();

    // TODO: Use semaphore to realize the flush of waiting for asynchronous
    // thread

    auto count = flush_count_.load();
    // Try to lock to ensure that the reading of fifo and writing of files in
    // the asynchronous thread have been completed
    do {
      need_flush_ = true;
      fifo_.InterruptOutput();
      std::this_thread::yield();
    } while (flush_count_ == count);
  }

  size_t InPacket(const void *buf, size_t num_elements) {
    return fifo_.InputPacketOrDrop(buf, num_elements);
  }

  size_t fifo_size() const { return fifo_.size(); }
  size_t fifo_num_dropped() const { return fifo_.num_dropped(); }
  size_t fifo_peak() const { return fifo_.peak(); }
  bool is_idle() const { return fifo_.empty(); }

 private:
  FifoPowerOfTwo fifo_;
  RotatingFile rotating_file_;
  std::unique_ptr<std::thread> async_thread_;

  // For flush
  std::time_t flush_period_sec_;
  std::atomic_size_t flush_count_{};
  std::atomic_bool need_flush_{false};

  std::atomic_bool should_exit_{false};
};

}  // namespace ulog
