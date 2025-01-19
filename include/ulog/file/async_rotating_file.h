//
// Created by fs on 2020-05-26.
//

#pragma once

#include <atomic>
#include <cstring>
#include <ctime>
#include <memory>
#include <string>
#include <thread>

#include "ulog/error.h"
#include "ulog/file/rotating_file.h"

namespace ulog {

template <typename Queue>
class AsyncRotatingFile {
 public:
  /**
   * Build a logger that asynchronously outputs data to a file
   * @param writer File writing instance, which may be compressed writing or direct writing
   * @param fifo_size Asynchronous first-in first-out buffer size
   * @param filename File name
   * @param max_files Number of files that can be divided
   * @param rotate_on_open Whether to rotate the file when opening
   * @param max_flush_period_sec Maximum file flush period (some file systems and platforms only refresh once every 60s
   * by default, which is too slow)
   */
  AsyncRotatingFile(std::unique_ptr<WriterInterface> &&writer, const size_t fifo_size, const std::string &filename,
                    const std::size_t max_files, const bool rotate_on_open, const std::time_t max_flush_period_sec)
      : umq_(Queue::Create(fifo_size)), rotating_file_(std::move(writer), filename, max_files, rotate_on_open) {
    auto async_thread_function = [=, this]() {
      std::time_t last_flush_time = std::time(nullptr);
      typename Queue::Consumer reader(umq_->shared_from_this());
      while (!should_exit_) {
        auto data_packet = reader.ReadOrWait(std::chrono::milliseconds(1000), [&] { return should_exit_.load(); });
        while (const auto data = data_packet.next()) {
          auto status = rotating_file_.SinkIt(data.data, data.size);
          if (!status) {
            ULOG_ERROR("Failed to write to file: %s", status.ToString().c_str());
          }
        }
        reader.Release(data_packet);

        // Flush
        std::time_t const cur_time = std::time(nullptr);
        if (cur_time - last_flush_time >= max_flush_period_sec) {
          last_flush_time = cur_time;
          auto status = rotating_file_.Flush();
          if (!status) {
            ULOG_ERROR("Failed to flush file: %s", status.ToString().c_str());
          }
        }
      }
    };
    async_thread_ = std::make_unique<std::thread>(async_thread_function);
  }

  AsyncRotatingFile(const AsyncRotatingFile &) = delete;
  AsyncRotatingFile &operator=(const AsyncRotatingFile &) = delete;

  Status Flush() const {
    umq_->Flush();
    return rotating_file_.Flush();
  }

  ~AsyncRotatingFile() {
    umq_->Flush(std::chrono::seconds(5));
    should_exit_ = true;
    umq_->Notify();
    if (async_thread_) async_thread_->join();
  }

  typename Queue::Producer CreateProducer() const { return typename Queue::Producer(umq_->shared_from_this()); }

  size_t InPacket(const void *buf, const size_t num_elements,
                  const std::chrono::milliseconds wait_time = std::chrono::milliseconds(1000)) const {
    typename Queue::Producer writer(umq_->shared_from_this());
    uint8_t *buffer = writer.ReserveOrWaitFor(num_elements, wait_time);
    if (buffer) {
      memcpy(buffer, buf, num_elements);
      writer.Commit(buffer, num_elements);
      return num_elements;
    }
    return 0;
  }

 private:
  std::shared_ptr<Queue> umq_;
  RotatingFile rotating_file_;
  std::unique_ptr<std::thread> async_thread_;

  std::atomic_bool should_exit_{false};
};

}  // namespace ulog
