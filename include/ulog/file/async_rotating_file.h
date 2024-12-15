//
// Created by fs on 2020-05-26.
//

#pragma once

#include <atomic>
#include <ctime>
#include <string>
#include <utility>

#include "ulog/file/rotating_file.h"
#include "ulog/queue/fifo_power_of_two.h"
#include "ulog/queue/mpsc_ring.h"

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
   */
  AsyncRotatingFile(const size_t fifo_size, std::string filename, const std::size_t max_file_size,
                    const std::size_t max_files, const std::time_t max_flush_period_sec = 0)
      : umq_(mpsc::Mq::Create(fifo_size)), rotating_file_(std::move(filename), max_file_size, max_files) {
    auto async_thread_function = [=, this]() {
      std::time_t last_flush_time = std::time(nullptr);
      mpsc::Consumer reader(umq_->shared_from_this());
      while (!should_exit_) {
        mpsc::DataPacket ptr = reader.ReadOrWait(std::chrono::milliseconds(1000), [&] { return should_exit_.load(); });
        while (const auto data = ptr.next()) {
          rotating_file_.SinkIt(data.data, data.size);
        }
        reader.ReleasePacket();

        // Flush
        std::time_t const cur_time = std::time(nullptr);
        if (cur_time - last_flush_time >= max_flush_period_sec) {
          last_flush_time = cur_time;
          rotating_file_.Flush();
        }
      }
    };
    async_thread_ = std::unique_ptr<std::thread>{new std::thread{async_thread_function}};
  }

  AsyncRotatingFile(const AsyncRotatingFile &) = delete;
  AsyncRotatingFile &operator=(const AsyncRotatingFile &) = delete;

  void Flush() const {
    umq_->Flush();
    rotating_file_.Flush();
  }

  ~AsyncRotatingFile() {
    umq_->Flush(std::chrono::seconds(5));
    should_exit_ = true;
    umq_->Notify();
    if (async_thread_) async_thread_->join();
  }

  mpsc::Producer CreateProducer() const { return mpsc::Producer(umq_->shared_from_this()); }

  size_t InPacket(const void *buf, const size_t num_elements,
                  const std::chrono::milliseconds wait_time = std::chrono::milliseconds(1000)) const {
    mpsc::Producer writer(umq_->shared_from_this());
    uint8_t *buffer = writer.ReserveOrWaitFor(num_elements, wait_time);
    if (buffer) {
      memcpy(buffer, buf, num_elements);
      writer.Commit(num_elements);
      return num_elements;
    }
    return 0;
  }

 private:
  std::shared_ptr<mpsc::Mq> umq_;
  RotatingFile rotating_file_;
  std::unique_ptr<std::thread> async_thread_;

  std::atomic_bool should_exit_{false};
};

}  // namespace ulog
