//
// Created by fs on 2020-05-25.
//

#pragma once

#include <string>

#include "ulog/status.h"

namespace ulog::file {

class FileWriterBase {
 public:
  FileWriterBase() = default;
  virtual ~FileWriterBase() = default;

  // Disable copy and assign.
  FileWriterBase(const FileWriterBase&) = delete;
  FileWriterBase& operator=(const FileWriterBase&) = delete;

  static constexpr size_t kNoLimit = static_cast<size_t>(-1);

  /**
   * Open the file.
   * @param filename File name.
   * @param truncate If true, the file will be truncated.
   * @param limit File write data size limit
   * @return 0 if success
   * @return Negative numbers are errors, use Status to judge
   */
  virtual Status Open(const std::string& filename, bool truncate, size_t limit) = 0;

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

  /**
   * Get the current file write position(filesize).
   * @return The current file write position.
   */
  virtual size_t TellP() = 0;
};
}  // namespace ulog::file
