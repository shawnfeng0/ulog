//
// Created by fs on 2020-05-26.
//

#pragma once

#include <atomic>
#include <cstring>
#include <list>
#include <memory>
#include <thread>

#include "ulog/error.h"
#include "ulog/file/sink_base.h"
#include "ulog/status.h"

namespace ulog::file {

template <typename Queue>
class SinkAsyncWrapper final : public SinkBase {
 public:
  /**
   * Build a logger that asynchronously outputs data to a file
   * @param fifo_size Asynchronous first-in first-out buffer size
   * @param max_flush_period  Maximum file flush period (some file systems and platforms only refresh once every 60s by
   * default, which is too slow)
   * @param next_sink Next sinker
   * @param args More sinker
   */
  template <typename... Args>
  SinkAsyncWrapper(size_t fifo_size, std::chrono::milliseconds max_flush_period, std::unique_ptr<SinkBase> &&next_sink,
                   Args &&...args)
      : umq_(Queue::Create(fifo_size)) {
    sinks_.emplace_back(std::move(next_sink));
    (sinks_.emplace_back(std::forward<Args>(args)), ...);

    auto async_thread_function = [max_flush_period, this] {
      auto last_flush_time = std::chrono::steady_clock::now();
      bool need_wait_flush = false;  // Whether to wait for the next flush
      typename Queue::Consumer reader(umq_->shared_from_this());

      while (!should_exit_) {
        bool flush_now = false;

        auto data_packet = [&] {
          if (should_flush_) {
            should_flush_ = false;
            flush_now = true;
            return reader.Read();
          }
          return need_wait_flush
                     ? reader.ReadOrWait(max_flush_period, [&] { return should_flush_.load() || should_exit_.load(); })
                     : reader.ReadOrWait([&] { return should_flush_.load() || should_exit_.load(); });
        }();

        while (const auto data = data_packet.next()) {
          auto status = SinkAll(data.data, data.size);
          if (!status) {
            ULOG_ERROR("Failed to sink: %s", status.ToString().c_str());
          }
        }
        reader.Release(data_packet);

        // Flush
        const auto cur_time = std::chrono::steady_clock::now();
        if (flush_now || (need_wait_flush && cur_time - last_flush_time >= max_flush_period)) {
          if (auto status = FlushAllSink(); !status) {
            ULOG_ERROR("Failed to flush file: %s", status.ToString().c_str());
            continue;
          }
          last_flush_time = cur_time;

          // Already flushed, only need to wait forever for the next data
          need_wait_flush = false;

        } else {
          // data has been written and the maximum waiting time is until the next flush
          need_wait_flush = true;
        }
      }
    };
    async_thread_ = std::make_unique<std::thread>(async_thread_function);
  }

  SinkAsyncWrapper(const SinkAsyncWrapper &) = delete;
  SinkAsyncWrapper &operator=(const SinkAsyncWrapper &) = delete;

  ~SinkAsyncWrapper() override {
    umq_->Flush(std::chrono::seconds(5));
    should_exit_ = true;
    umq_->Notify();
    if (async_thread_) async_thread_->join();
  }

  Status Flush() override {
    should_flush_.store(true);
    umq_->Flush();
    return Status::OK();
  }

  typename Queue::Producer CreateProducer() const { return typename Queue::Producer(umq_->shared_from_this()); }

  Status SinkIt(const void *data, const size_t len, std::chrono::milliseconds timeout) override {
    typename Queue::Producer writer(umq_->shared_from_this());
    if (auto *buffer = writer.ReserveOrWaitFor(len, timeout)) {
      memcpy(buffer, data, len);
      writer.Commit(buffer, len);
      return Status::OK();
    }
    return Status::Full();
  }

  Status SinkIt(const void *data, const size_t len) override {
    typename Queue::Producer writer(umq_->shared_from_this());
    if (auto *buffer = writer.Reserve(len)) {
      memcpy(buffer, data, len);
      writer.Commit(buffer, len);
      return Status::OK();
    }
    return Status::Full();
  }

 private:
  [[nodiscard]] Status SinkAll(const void *data, const size_t len) {
    Status result = Status::OK();
    for (auto sink = sinks_.begin(); sink != sinks_.end();) {
      if (const auto status = (*sink)->SinkIt(data, len); !status) {
        if (status.IsFull()) {
          sink = sinks_.erase(sink);
          continue;
        }
        result = status;
      }

      ++sink;
    }
    return result;
  }

  [[nodiscard]] Status FlushAllSink() const {
    Status result = Status::OK();
    for (auto &sink : sinks_) {
      if (const auto status = sink->Flush(); !status) {
        result = status;
      }
    }
    return result;
  }

  std::shared_ptr<Queue> umq_;
  std::list<std::unique_ptr<SinkBase>> sinks_;
  std::unique_ptr<std::thread> async_thread_;

  std::atomic_bool should_exit_{false};
  std::atomic_bool should_flush_{false};
};

}  // namespace ulog::file
