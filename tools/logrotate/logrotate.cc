#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <cstdlib>

#include "cmdline.h"
#include "ulog/file/async_rotating_file.h"
#include "ulog/ulog.h"

int main(int argc, char *argv[]) {
  gengetopt_args_info args_info{};

  if (cmdline_parser(argc, argv, &args_info) != 0) exit(1);

  const int fifo_size = std::max(args_info.fifo_size_arg, 16 * 1024);

  const ulog::AsyncRotatingFile async_rotate(fifo_size, args_info.file_path_arg, args_info.file_size_arg,
                                             args_info.file_number_arg, args_info.flush_interval_arg);

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

  const size_t reverse_size = std::min(4 * 1024, fifo_size / 8);
  auto writer = async_rotate.CreateProducer();

  pollfd fds{.fd = STDIN_FILENO, .events = POLLIN, .revents = 0};
  while (poll(&fds, 1, -1) >= 0) {
    uint8_t *buffer = writer.ReserveOrWait(reverse_size);

    const auto real_size = read(STDIN_FILENO, buffer, reverse_size);

    // End of input
    if (real_size <= 0) {
      break;
    }

    if (args_info.stdout_flag) {
      write(STDOUT_FILENO, buffer, real_size);
    }
    writer.Commit(real_size);
  }

  return 0;
}
