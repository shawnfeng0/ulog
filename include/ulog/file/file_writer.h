//
// Created by fs on 2020-05-25.
//

#pragma once

#include "file.h"
#include "file_writer_base.h"

namespace ulog::file {

class FileWriter final : public FileWriterBase {
 public:
  explicit FileWriter() : file_limit_size_(kNoLimit) {}
  ~FileWriter() override { FileWriter::Close(); }

  Status Open(const std::string &filename, const bool truncate, size_t limit) override {
    if (file_ != nullptr) {
      return Status::Corruption("File already opened!", filename);
    }

    // create containing folder if not exists already.
    if (!file::create_dir(file::dir_name(filename))) {
      return Status::Corruption("Error creating directory", filename);
    }

    file_ = fopen(filename.c_str(), truncate ? "wb" : "ab");
    if (!file_) {
      return Status::IOError("Error opening file", filename);
    }

    file_write_size_ = truncate ? 0 : file::filesize(file_);
    file_limit_size_ = limit;
    return Status::OK();
  }

  Status Write(const void *data, const size_t length) override {
    if (!file_) {
      return Status::Corruption("Not opened");
    }

    if (file_write_size_ + length > file_limit_size_) {
      return Status::Full();
    }

    if (std::fwrite(data, 1, length, file_) != length) {
      return Status::IOError("Error writing to file");
    }

    file_write_size_ += length;
    return Status::OK();
  }

  Status Flush() override {
    if (!file_) {
      return Status::Corruption("Not opened");
    }

    fflush(file_);
    return Status::OK();
  }

  Status Close() override {
    if (!file_) {
      return Status::Corruption("Not opened");
    }

    fclose(file_);
    file_ = nullptr;

    return Status::OK();
  }

 private:
  // config
  size_t file_limit_size_;

  std::FILE *file_{nullptr};
  size_t file_write_size_{0};
};
}  // namespace ulog::file
