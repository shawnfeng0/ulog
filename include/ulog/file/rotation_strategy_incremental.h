#pragma once

#include <fstream>
#include <iostream>
#include <string>
#include <utility>

#include "rotation_strategy_interface.h"
#include "ulog/status.h"

namespace ulog::file {

class RotationStrategyIncremental final : public RotationStrategyInterface {
 public:
  RotationStrategyIncremental(std::string basename, std::string ext, const size_t max_files)
      : basename_(std::move(basename)), ext_(std::move(ext)), max_files_(max_files == 0 ? 1 : max_files) {
    if (const auto status = ReadNumberFile(get_number_filename(), &final_number_); !status.ok()) {
      // Maybe first write
      final_number_ = 0;
    }
  }

  // Example:
  // max_files == 10;
  // final_num == 29 + 1;
  // remove file: log-20.txt
  // remain files: log-21.txt log-22.txt ... log-29.txt
  // Next, call LatestFilename() to get log-30.txt for writing
  Status Rotate() override {
    ++final_number_;

    if (final_number_ >= max_files_) {
      std::remove(get_filename(final_number_ - max_files_).c_str());

      // Try to clear the files that were previously configured too much
      size_t not_exists = 0;
      for (size_t i = final_number_ - max_files_; i != 0; --i) {
        std::string filename = get_filename(i);
        if (!path_exists(filename)) {
          if (++not_exists == 2) break;
          continue;
        }
        std::remove(filename.c_str());
      }
      std::remove(get_filename(0).c_str());
    }
    return WriteNumberFile(get_number_filename(), final_number_);
  }

  std::string LatestFilename() override { return basename_ + "-" + std::to_string(final_number_) + ext_; }

 private:
  [[nodiscard]] std::string get_number_filename() const { return basename_ + ext_ + ".latest"; }
  [[nodiscard]] std::string get_filename(const size_t index) const {
    return basename_ + "-" + std::to_string(index) + ext_;
  }

  static Status ReadNumberFile(const std::string& filename, size_t* const final_number) {
    std::ifstream input_file(filename);
    if (!input_file.is_open()) {
      *final_number = 0;
      return Status::IOError("Failed to open file: " + filename);
    }

    input_file >> *final_number;
    input_file.close();
    return Status::OK();
  }

  // Directly writes the latest number to the file
  static Status WriteNumberFile(const std::string& filename, const size_t number) {
    std::ofstream output_file(filename);

    if (!output_file.is_open()) {
      return Status::IOError("Failed to write number to: " + filename);
    }

    output_file << number;
    output_file.close();
    return Status::OK();
  }

  // config
  const std::string basename_;
  const std::string ext_;
  const size_t max_files_;

  size_t final_number_ = 0;
};

}  // namespace ulog::file