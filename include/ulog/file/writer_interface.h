//
// Created by fs on 2020-05-25.
//

#pragma once

#include <string>

#include "ulog/status.h"

namespace ulog {

class WriterInterface {
 public:
  WriterInterface() = default;
  virtual ~WriterInterface() = default;

  // Disable copy and assign.
  WriterInterface(const WriterInterface&) = delete;
  WriterInterface& operator=(const WriterInterface&) = delete;

  /**
   * Open the file.
   * @param filename File name.
   * @param truncate If true, the file will be truncated.
   * @return 0 if success
   * @return Negative numbers are errors, use Status to judge
   */
  virtual Status Open(const std::string& filename, bool truncate) = 0;

  /**
   * Flush the file.
   * @return 0 if success
   * @return Negative numbers are errors, use Status to judge
   */
  virtual Status Flush() = 0;

  /**
   * Close the file.
   * @return 0 if success
   * @return Negative numbers are errors, use Status to judge
   */
  virtual Status Close() = 0;

  /**
   * Write data to the file.
   * @param data The data to write.
   * @param len The length of the data.
   * @return Status::OK() if success
   * @return Status::Full() if the file is full
   */
  virtual Status Write(const void* data, std::size_t len) = 0;
};
}  // namespace ulog
