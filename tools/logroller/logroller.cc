#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <map>
#include <sstream>

#include "cmdline.h"
#include "ulog/file/file_writer_zstd.h"
#include "ulog/file/sink_async_wrapper.h"
#include "ulog/file/sink_limit_size_file.h"
#include "ulog/file/sink_rotating_file.h"
#include "ulog/queue/spsc_ring.h"

#define ZSTD_DEFAULT_LEVEL 3

static auto to_bytes(const std::string &size_str) {
  size_t i = 0;
  const auto size = std::stoull(size_str, &i);

  std::string unit = size_str.substr(i);
  std::transform(unit.begin(), unit.end(), unit.begin(), ::tolower);

  if (unit == "k" || unit == "kb" || unit == "kib") return size << 10;
  if (unit == "m" || unit == "mb" || unit == "mib") return size << 20;
  if (unit == "g" || unit == "gb" || unit == "gib") return size << 30;

  return size;
}

static std::chrono::milliseconds to_chrono_time(const std::string &time_str) {
  size_t i = 0;
  const auto size = std::stoull(time_str, &i);

  std::string unit = time_str.substr(i);
  std::transform(unit.begin(), unit.end(), unit.begin(), ::tolower);

  if (unit == "ms") return std::chrono::milliseconds{size};
  if (unit == "s" || unit == "sec") return std::chrono::seconds{size};
  if (unit == "min") return std::chrono::minutes{size};
  if (unit == "hour") return std::chrono::hours{size};

  return std::chrono::seconds{size};
}

static std::map<std::string, std::string> ParseParametersMap(const std::string &input) {
  std::map<std::string, std::string> params;
  std::string item;
  std::stringstream ss(input);

  while (std::getline(ss, item, ',')) {
    const std::size_t pos = item.find('=');
    if (pos != std::string::npos) {
      const std::string key = item.substr(0, pos);
      const std::string value = item.substr(pos + 1);
      params[key] = value;
    }
  }

  return params;
}

int main(const int argc, char *argv[]) {
  gengetopt_args_info args_info{};

  if (cmdline_parser(argc, argv, &args_info) != 0) exit(1);

  const auto fifo_size = std::max(to_bytes(args_info.fifo_size_arg), 16ULL * 1024);
  const auto max_flush_period = to_chrono_time(args_info.flush_interval_arg);
  const auto rotation_strategy = std::string(args_info.rotation_strategy_arg) == "incremental"
                                     ? ulog::file::RotationStrategy::kIncrement
                                     : ulog::file::RotationStrategy::kRename;
  std::string filename = args_info.file_path_arg;

  std::unique_ptr<ulog::file::FileWriterBase> file_writer;
  // Zstd compression
  if (args_info.zstd_compress_flag) {
    // Append .zst extension if not present
    std::string basename, ext;
    std::tie(basename, ext) = ulog::file::SplitByExtension(filename);
    if (ext.find(".zst") == std::string::npos) filename += ".zst";

    // Parse zstd parameters
    if (args_info.zstd_params_given) {
      const std::map<std::string, std::string> result = ParseParametersMap(args_info.zstd_params_arg);
      file_writer = std::make_unique<ulog::file::FileWriterZstd>(
          result.count("level") ? std::stoi(result.at("level")) : ZSTD_DEFAULT_LEVEL,
          result.count("window-log") ? std::stoi(result.at("window-log")) : 0,
          result.count("chain-log") ? std::stoi(result.at("chain-log")) : 0,
          result.count("hash-log") ? std::stoi(result.at("hash-log")) : 0,
          result.count("search-log") ? std::stoi(result.at("search-log")) : 0,
          result.count("min-match") ? std::stoi(result.at("min-match")) : 0,
          result.count("target-length") ? std::stoi(result.at("target-length")) : 0,
          result.count("strategy") ? std::stoi(result.at("strategy")) : 0);

      // No zstd parameters
    } else {
      file_writer = std::make_unique<ulog::file::FileWriterZstd>();
    }

    // No compression
  } else {
    file_writer = std::make_unique<ulog::file::FileWriter>();
  }

  std::unique_ptr<ulog::file::SinkBase> rotating_file = std::make_unique<ulog::file::SinkRotatingFile>(
      std::move(file_writer), filename, to_bytes(args_info.file_size_arg), args_info.max_files_arg,
      args_info.rotate_first_flag, rotation_strategy, nullptr);
  const ulog::file::SinkAsyncWrapper<ulog::spsc::Mq<>> async_rotate(fifo_size, max_flush_period,
                                                                    std::move(rotating_file));

  cmdline_parser_free(&args_info);

  // Set O_NONBLOCK flag
  {
    const int flag = fcntl(STDIN_FILENO, F_GETFL);
    if (flag < 0) {
      ULOG_ERROR("fcntl failed");
      return -1;
    }
    fcntl(STDIN_FILENO, F_SETFL, flag | O_NONBLOCK);
  }

  const size_t reverse_size = std::min(4ULL * 1024, fifo_size / 8);
  auto writer = async_rotate.CreateProducer();

  pollfd fds{.fd = STDIN_FILENO, .events = POLLIN, .revents = 0};
  while (poll(&fds, 1, -1) >= 0) {
    auto *buffer = writer.ReserveOrWait(reverse_size);

    const auto real_size = read(STDIN_FILENO, buffer, reverse_size);

    // End of input
    if (real_size <= 0) {
      writer.Commit(buffer, 0);
      break;
    }

    writer.Commit(buffer, real_size);
  }
  return 0;
}
