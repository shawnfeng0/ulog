//
// Created by fs on 2020-05-25.
//

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "file_writer_buffered_io.h"
#include "rotation_strategy_incremental.h"
#include "rotation_strategy_rename.h"
#include "sink_base.h"

namespace ulog::file {

enum RotationStrategy {
  kRename = 1,
  kIncrement = 0,
};

class SinkRotatingFile final : public SinkBase {
 public:
  /**
   * Outputs data to a file and rotates the file
   * @param writer File writer
   * @param filename File name
   * @param file_size File write data size limit
   * @param max_files Number of files that can be divided
   * @param rotate_on_open Whether to rotate the file when opening
   * @param rotation_strategy Rotation strategy
   * @param cb_file_head Called to obtain header data before writing each file
   */
  SinkRotatingFile(std::unique_ptr<FileWriterBase> &&writer, std::string filename, const std::size_t file_size,
               const std::size_t max_files, const bool rotate_on_open, const RotationStrategy rotation_strategy,
               std::function<std::vector<char>()> &&cb_file_head)
      : file_size_(file_size),
        max_files_(max_files),
        writer_(std::move(writer)),
        filename_(std::move(filename)),
        cb_file_head_(std::move(cb_file_head)) {
    auto [basename, ext] = SplitByExtension(filename_);
    if (rotation_strategy == kIncrement) {
      rotator_ = std::make_unique<RotationStrategyIncremental>(basename, ext, max_files_);
    } else {
      rotator_ = std::make_unique<RotationStrategyRename>(basename, ext, max_files_);
    }

    if (rotate_on_open) {
      rotator_->Rotate();
    }

    if (const auto status = writer_->Open(rotator_->LatestFilename(), rotate_on_open, file_size_); !status) {
      ULOG_ERROR("Failed to open file: %s", status.ToString().c_str());
    }

    if (cb_file_head_) {
      const auto head_data = cb_file_head_();
      if (const auto status = writer_->Write(head_data.data(), head_data.size()); !status) {
        ULOG_ERROR("Failed to write file header");
      }
    }
  }

  Status SinkIt(const void *data, const size_t len, [[maybe_unused]] std::chrono::milliseconds timeout) override {
    return SinkIt(data, len);
  }

  Status SinkIt(const void *data, const size_t len) override {
    Status status = writer_->Write(data, len);

    if (status.IsFull()) {
      writer_->Close();
      rotator_->Rotate();

      status = writer_->Open(rotator_->LatestFilename(), true, file_size_);
      if (!status) {
        return status;
      }

      if (cb_file_head_) {
        const auto head_data = cb_file_head_();
        status = writer_->Write(head_data.data(), head_data.size());
        if (!status) {
          return status;
        }
      }

      return writer_->Write(data, len);
    }

    return status;
  }

  [[nodiscard]] Status Flush() override { return writer_->Flush(); }

 private:
  const std::size_t file_size_;
  const std::size_t max_files_;
  std::unique_ptr<FileWriterBase> writer_;

  std::string filename_;
  std::unique_ptr<RotationStrategyBase> rotator_;
  std::function<std::vector<char>()> cb_file_head_;
};

}  // namespace ulog::file
