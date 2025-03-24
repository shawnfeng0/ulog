//
// Created by fs on 2020-05-25.
//

#pragma once

#include <functional>
#include <string>
#include <vector>
#include <memory>

#include "file_writer_buffered_io.h"
#include "rotation_strategy_incremental.h"
#include "rotation_strategy_rename.h"
#include "sink_base.h"

namespace ulog::file {

class SinkLimitSizeFile final : public SinkBase {
 public:
  /**
   * Outputs data to a file and rotates the file
   * @param writer File writer
   * @param filename File name
   * @param file_size File write data size limit
   */
  SinkLimitSizeFile(std::unique_ptr<FileWriterBase> &&writer, std::string filename, const std::size_t file_size)
      : file_size_(file_size),
        writer_(std::move(writer)),
        filename_(std::move(filename)) {
    writer_->Open(filename_, true, file_size_);
  }

  Status SinkIt(const void *data, const size_t len, [[maybe_unused]] std::chrono::milliseconds timeout) override {
    return SinkIt(data, len);
  }

  Status SinkIt(const void *data, const size_t len) override {
    return writer_->Write(data, len);
  }

  [[nodiscard]] Status Flush() override { return writer_->Flush(); }

 private:
  const std::size_t file_size_;
  std::unique_ptr<FileWriterBase> writer_;
  std::string filename_;
};

}  // namespace ulog::file
