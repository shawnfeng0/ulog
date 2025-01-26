//
// Created by shawnfeng on 25-1-27.
//

#pragma once

#include "ulog/status.h"

namespace ulog::file {

class SinkBase {
public:
  SinkBase() = default;
  virtual ~SinkBase() = default;

  // Disable copy and assign.
  SinkBase(const SinkBase&) = delete;
  SinkBase& operator=(const SinkBase&) = delete;

  /**
   * Sink data to next sinker
   * @param data The data to write.
   * @param len The length of the data.
   * @return Status::OK() if success
   * @return Status::Full() if the file is full
   */
  virtual Status SinkIt(const void* data, size_t len) = 0;
  virtual Status SinkIt(const void* data, size_t len, std::chrono::milliseconds timeout) = 0;

  /**
   * Flush the file.
   * @return 0 if success
   * @return Negative numbers are errors, use Status to judge
   */
  virtual Status Flush() = 0;
};
}
