//
// Created by fs on 2020-05-25.
//

#pragma once

#include <string>
#include <utility>

#include "file_writer.h"
#include "rotation_strategy_incremental.h"
#include "rotation_strategy_rename.h"

namespace ulog::file {

enum RotationStrategy {
  kRename = 1,
  kIncrement = 0,
};

class RotatingFile {
 public:
  RotatingFile(std::unique_ptr<WriterInterface> &&writer, std::string filename, const std::size_t max_files,
               const bool rotate_on_open, const RotationStrategy rotation_strategy,
               std::function<std::vector<char>()> &&cb_file_head)
      : max_files_(max_files),
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

    writer_->Open(rotator_->LatestFilename(), rotate_on_open);

    if (cb_file_head_) {
      const auto head_data = cb_file_head_();
      if (auto status = writer_->Write(head_data.data(), head_data.size()); !status) {
        ULOG_ERROR("Failed to write file header");
      }
    }
  }

  Status SinkIt(const void *buffer, const size_t length) const {
    Status status = writer_->Write(buffer, length);

    if (status.IsFull()) {
      writer_->Close();
      rotator_->Rotate();

      status = writer_->Open(rotator_->LatestFilename(), true);
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

      return writer_->Write(buffer, length);
    }

    return status;
  }

  [[nodiscard]] Status Flush() const { return writer_->Flush(); }

 private:
  const std::size_t max_files_;
  std::unique_ptr<WriterInterface> writer_;

  std::string filename_;
  std::unique_ptr<RotationStrategyInterface> rotator_;
  std::function<std::vector<char>()> cb_file_head_;
};

}  // namespace ulog::file
