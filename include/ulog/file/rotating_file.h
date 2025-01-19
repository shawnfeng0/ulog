//
// Created by fs on 2020-05-25.
//

#pragma once

#include <fstream>
#include <string>
#include <utility>

#include "file.h"
#include "file_writer.h"

namespace ulog {

class RotatingFile {
 public:
  RotatingFile(std::unique_ptr<WriterInterface> &&writer, std::string filename, const std::size_t max_files,
               const bool rotate_on_open)
      : max_files_(max_files), writer_(std::move(writer)), filename_(std::move(filename)) {
    if (rotate_on_open) {
      file::RotateFiles(filename, max_files_);
    }

    writer_->Open(filename_, rotate_on_open);
  }

  Status SinkIt(const void *buffer, const size_t length) const {
    Status status = writer_->Write(buffer, length);

    if (status.IsFull()) {
      writer_->Close();
      file::RotateFiles(filename_, max_files_);
      status = writer_->Open(filename_, true);

      if (!status) {
        return status;
      }

      return writer_->Write(buffer, length);
    }

    return status;
  }

  Status Flush() const { return writer_->Flush(); }

 private:
  const std::size_t max_files_;
  std::unique_ptr<WriterInterface> writer_;

  std::string filename_;
};

}  // namespace ulog
