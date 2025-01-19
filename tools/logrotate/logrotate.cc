#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <map>

#include "cmdline.h"
#include "ulog/file/async_rotating_file.h"
#include "ulog/file/zstd_file_writer.h"
#include "ulog/queue/mpsc_ring.h"
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
  const auto file_size = to_bytes(args_info.file_size_arg);
  const auto flush_interval = to_chrono_time(args_info.flush_interval_arg);
  std::string filepath = args_info.file_path_arg;

  std::unique_ptr<ulog::WriterInterface> file_writer;
  // Zstd compression
  if (args_info.zstd_compress_flag) {
    // Append .zst extension if not present
    std::string basename, ext;
    std::tie(basename, ext) = ulog::file::SplitByExtension(filepath);
    if (ext != ".zst") filepath += ".zst";

    // Parse zstd parameters
    if (args_info.zstd_params_given) {
      const std::map<std::string, std::string> result = ParseParametersMap(args_info.zstd_params_arg);
      file_writer = std::make_unique<ulog::ZstdLimitFile>(
          file_size, result.count("level") ? std::stoi(result.at("level")) : ZSTD_DEFAULT_LEVEL,
          result.count("window-log") ? std::stoi(result.at("window-log")) : 0,
          result.count("chain-log") ? std::stoi(result.at("chain-log")) : 0,
          result.count("hash-log") ? std::stoi(result.at("hash-log")) : 0);

      // No zstd parameters
    } else {
      file_writer = std::make_unique<ulog::ZstdLimitFile>(file_size);
    }

    // No compression
  } else {
    file_writer = std::make_unique<ulog::FileLimitWriter>(file_size);
  }

  const ulog::AsyncRotatingFile<ulog::spsc::Mq<uint8_t>> async_rotate(
      std::move(file_writer), fifo_size, filepath, args_info.file_number_arg, args_info.rotate_first_flag,
      flush_interval);

  cmdline_parser_free(&args_info);

  // Set O_NONBLOCK flag
  {
    const int flag = fcntl(STDIN_FILENO, F_GETFL);
    if (flag < 0) {
      perror("fcntl");
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
