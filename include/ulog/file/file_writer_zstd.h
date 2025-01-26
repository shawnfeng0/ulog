#pragma once
#include <zstd.h>

#include <cstdint>
#include <vector>

#include "file.h"
#include "file_writer_base.h"

namespace ulog::file {

class FileWriterZstd final : public FileWriterBase {
 public:
  explicit FileWriterZstd(const int level = 3, const int window_log = 14, const int chain_log = 14,
                          const int hash_log = 15, const int search_log = 2, const int min_match = 4,
                          const int target_length = 0, const int strategy = 2, const size_t max_frame_in = 8 << 20)
      : config_file_limit_size_(kNoLimit), config_zstd_max_frame_in_(max_frame_in) {
    zstd_out_buffer_.resize(16 * 1024);

    zstd_cctx_ = ZSTD_createCCtx();
    assert(zstd_cctx_ != nullptr);

    ZSTD_CCtx_setParameter(zstd_cctx_, ZSTD_c_compressionLevel, level);
    ZSTD_CCtx_setParameter(zstd_cctx_, ZSTD_c_windowLog, window_log);
    ZSTD_CCtx_setParameter(zstd_cctx_, ZSTD_c_chainLog, chain_log);
    ZSTD_CCtx_setParameter(zstd_cctx_, ZSTD_c_hashLog, hash_log);
    ZSTD_CCtx_setParameter(zstd_cctx_, ZSTD_c_searchLog, search_log);
    ZSTD_CCtx_setParameter(zstd_cctx_, ZSTD_c_minMatch, min_match);
    ZSTD_CCtx_setParameter(zstd_cctx_, ZSTD_c_targetLength, target_length);
    ZSTD_CCtx_setParameter(zstd_cctx_, ZSTD_c_strategy, strategy);
  }

  ~FileWriterZstd() override {
    FileWriterZstd::Close();
    ZSTD_freeCCtx(zstd_cctx_);
  }

  Status Open(const std::string& filename, const bool truncate, const size_t limit) override {
    if (!zstd_cctx_) {
      return Status::Corruption("Error creating ZSTD context");
    }

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

    file_write_compress_size_ = truncate ? 0 : file::filesize(file_);
    zstd_frame_in_ = 0;
    config_file_limit_size_ = limit;
    return Status::OK();
  }

  Status Write(const void* data, const size_t length) override {
    if (file_ == nullptr) {
      return Status::Corruption("Not opened");
    }

    if (file_write_compress_size_ + ZSTD_compressBound(length) + zstd_header_size() > config_file_limit_size_) {
      return Status::Full();
    }

    const ZSTD_EndDirective mode =
        (zstd_frame_in_ + length >= config_zstd_max_frame_in_) ? ZSTD_e_end : ZSTD_e_continue;
    auto status = WriteCompressData(data, length, mode);
    if (status) {
      zstd_frame_in_ = mode == ZSTD_e_end ? 0 : zstd_frame_in_ + length;
    }
    return status;
  }

  Status Flush() override {
    if (!file_) {
      return Status::Corruption("Not opened");
    }

    if (zstd_frame_in_ > 0) {
      auto status = WriteCompressData(nullptr, 0, ZSTD_e_end);
      if (!status) return status;

      zstd_frame_in_ = 0;
    }

    std::fflush(file_);
    return Status::OK();
  }

  Status Close() override {
    if (!file_) {
      return Status::Corruption("Not opened");
    }

    Status result = Flush();

    fclose(file_);
    file_ = nullptr;

    return result;
  }

 private:
  static size_t zstd_header_size() {
    static size_t header_size = ZSTD_CStreamOutSize() > ZSTD_compressBound(ZSTD_CStreamInSize())
                                    ? ZSTD_CStreamOutSize() - ZSTD_compressBound(ZSTD_CStreamInSize())
                                    : 0;
    return header_size;
  }

  /* Compress until the input buffer is empty, each time flushing the output. */
  Status WriteCompressData(const void* data, const size_t length, const ZSTD_EndDirective mode) {
    size_t remaining = 0;
    ZSTD_inBuffer in_buffer = {data, length, 0};
    do {
      ZSTD_outBuffer out_buffer{zstd_out_buffer_.data(), zstd_out_buffer_.size(), 0};
      remaining = ZSTD_compressStream2(zstd_cctx_, &out_buffer, &in_buffer, mode);
      if (ZSTD_isError(remaining)) {
        return Status::Corruption(ZSTD_getErrorName(remaining));
      }

      if (std::fwrite(out_buffer.dst, 1, out_buffer.pos, file_) != out_buffer.pos) {
        return Status::IOError("Error writing to file");
      }

      file_write_compress_size_ += out_buffer.pos;
    } while (remaining != 0);

    return Status::OK();
  }

  // config
  size_t config_file_limit_size_;
  const size_t config_zstd_max_frame_in_;

  FILE* file_ = nullptr;
  size_t file_write_compress_size_{0};

  ZSTD_CCtx* zstd_cctx_ = nullptr;
  size_t zstd_frame_in_{0};
  std::vector<uint8_t> zstd_out_buffer_;
};

}  // namespace ulog::file
