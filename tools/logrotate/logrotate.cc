#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>

#include "cmdline.h"
#include "ulog/file/async_rotating_file.h"
#include "ulog/queue/mpsc_ring.h"
#include "ulog/queue/spsc_ring.h"

static auto to_bytes(const std::string &sizeStr) {
  size_t i = 0;
  const auto size = std::stoull(sizeStr, &i);

  std::string unit = sizeStr.substr(i);
  std::transform(unit.begin(), unit.end(), unit.begin(), ::tolower);

  if (unit == "k" || unit == "kb") return size << 10;
  if (unit == "m" || unit == "mb") return size << 20;
  if (unit == "g" || unit == "gb") return size << 30;

  return size;
}

int main(const int argc, char *argv[]) {
  gengetopt_args_info args_info{};

  if (cmdline_parser(argc, argv, &args_info) != 0) exit(1);

  const auto fifo_size = std::max(to_bytes(args_info.fifo_size_arg), 16ULL * 1024);
  const auto file_size = to_bytes(args_info.file_size_arg);

  const ulog::AsyncRotatingFile<ulog::spsc::Mq<uint8_t>> async_rotate(
      fifo_size, args_info.file_path_arg, file_size, args_info.file_number_arg, args_info.rotate_first_flag,
      args_info.flush_interval_arg);

  cmdline_parser_free(&args_info); /* release allocated memory */

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
