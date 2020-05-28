//
// Created by fs on 2020-05-25.
//

#ifndef ULOG_INCLUDE_ULOG_HELPER_ROTATING_FILE_H_
#define ULOG_INCLUDE_ULOG_HELPER_ROTATING_FILE_H_

#include <fstream>
#include <string>
#include <tuple>
#include <utility>

#include "ulog/helper/file_writer.h"

namespace ulog {

//
// Reference: https://github.com/gabime/spdlog
// Rotating file sink based on size
//
class RotatingFile {
 public:
  RotatingFile(std::string base_filename, std::size_t max_size,
               std::size_t max_files, bool rotate_on_open = false)
      : base_filename_(std::move(base_filename)),
        max_size_(max_size),
        max_files_(max_files) {
    file_writer_.Open(CalcFilename(base_filename_, 0));
    current_size_ = file_writer_.size();  // expensive. called only once
    if (rotate_on_open && current_size_ > 0) {
      Rotate();
    }
  }
  std::string filename() { return file_writer_.filename(); }

  void SinkIt(const void *buffer, size_t size) {
    current_size_ += size;
    if (current_size_ > max_size_) {
      Rotate();
      current_size_ = size;
    }
    file_writer_.Write(buffer, size);
  }

  void Flush() { file_writer_.Flush(); }

 private:
  using filename_t = std::string;

  // Rotate files:
  // log.txt -> log.1.txt
  // log.1.txt -> log.2.txt
  // log.2.txt -> log.3.txt
  // log.3.txt -> delete
  void Rotate() {
    file_writer_.Close();

    for (auto i = max_files_; i > 1; --i) {
      filename_t src = CalcFilename(base_filename_, i - 1);
      if (!file::path_exists(src)) {
        continue;
      }
      filename_t target = CalcFilename(base_filename_, i);
      RenameFile(src, target);
    }

    // In order to use tail -f to read files in real time, need to use copy
    // instead of rename
    CopyFile(base_filename_, CalcFilename(base_filename_, 1));

    file_writer_.Reopen(true);
  }

  static std::string CalcFilename(const std::string &filename,
                                  std::size_t index) {
    if (index == 0u) {
      return filename;
    }

    filename_t basename, ext;
    std::tie(basename, ext) = FileWriter::SplitByExtension(filename);
    return basename + "." + std::to_string(index) + ext;
  }

  // Copy file
  // Reference:
  // https://stackoverflow.com/questions/10195343/copy-a-file-in-a-sane-safe-and-efficient-way
  static void CopyFile(const std::string &src, const std::string &dst) {
    std::ifstream src_in(src, std::ios::binary);
    std::ofstream dst_out(dst, std::ios::binary);

    dst_out << src_in.rdbuf();
  }

  // delete the target if exists, and rename the src file  to target
  // return true on success, false otherwise.
  static bool RenameFile(const std::string &src_filename,
                         const std::string &target_filename) {
    // try to delete the target file in case it already exists.
    (void)std::remove(target_filename.c_str());
    return std::rename(src_filename.c_str(), target_filename.c_str()) == 0;
  }

  std::string base_filename_;
  std::size_t max_size_;
  std::size_t max_files_;
  std::size_t current_size_;
  FileWriter file_writer_;
};

}  // namespace ulog

#endif  // ULOG_INCLUDE_ULOG_HELPER_ROTATING_FILE_H_
