#pragma once
#include <zstd.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "file.h"
#include "file_writer_base.h"
#include "file_writer_buffered_io.h"

namespace ulog::file {

class FileWriterZstd final : public FileWriterBase {
 public:
  explicit FileWriterZstd(std::unique_ptr<FileWriterBase>&& file_writer, const int level = 3, const int window_log = 14,
                          const int chain_log = 14, const int hash_log = 15, const int search_log = 2,
                          const int min_match = 4, const int target_length = 0, const int strategy = 2,
                          const size_t max_frame_in = 8 << 20)
      : config_file_limit_size_(kNoLimit), config_zstd_max_frame_in_(max_frame_in), file_(std::move(file_writer)) {
    zstd_out_buffer_.resize(16 * 1024);
    out_buffer_ = {zstd_out_buffer_.data(), zstd_out_buffer_.size(), 0};

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

    // create containing folder if not exists already.
    if (!create_dir(dir_name(filename))) {
      return Status::Corruption("Error creating directory", filename);
    }

    if (const auto status = file_->Open(filename, truncate, limit); !status) {
      return status;
    }

    zstd_frame_in_ = 0;
    config_file_limit_size_ = limit;
    return Status::OK();
  }

  Status Write(const void* data, const size_t length) override {
    if (file_->TellP() + ZSTD_compressBound(length) + zstd_header_size() > config_file_limit_size_) {
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
    if (zstd_frame_in_ > 0) {
      if (const auto status = WriteCompressData(nullptr, 0, ZSTD_e_end); !status) {
        return status;
      }

      zstd_frame_in_ = 0;
    }

    file_->Flush();
    return Status::OK();
  }

  Status Close() override {
    auto f_result = Flush();
    auto c_result = file_->Close();

    return std::move(f_result.ok() ? c_result : f_result);
  }

  size_t TellP() override { return file_->TellP(); }

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
      remaining = ZSTD_compressStream2(zstd_cctx_, &out_buffer_, &in_buffer, mode);
      if (ZSTD_isError(remaining)) {
        return Status::Corruption(ZSTD_getErrorName(remaining));
      }

      if (mode == ZSTD_e_end || out_buffer_.size == out_buffer_.pos) {
        if (const auto status = file_->Write(out_buffer_.dst, out_buffer_.pos); !status) {
          return Status::IOError("Error writing to file");
        }
        out_buffer_.pos = 0;
      }

    } while (remaining != 0);

    return Status::OK();
  }

  // config
  size_t config_file_limit_size_;
  const size_t config_zstd_max_frame_in_;

  std::unique_ptr<FileWriterBase> file_;
  ZSTD_CCtx* zstd_cctx_ = nullptr;
  size_t zstd_frame_in_{0};
  std::vector<uint8_t> zstd_out_buffer_;
  ZSTD_outBuffer out_buffer_{};
};

}  // namespace ulog::file
