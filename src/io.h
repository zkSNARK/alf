//
// io.h -- reading the input and writing the kept lines, per operating system.  io_posix.cpp is
// for Linux, macOS and other POSIX systems (and Emscripten); io_windows.cpp for Windows.  The
// block reading for stdin and pipes, which is the same everywhere, is in io_stream.cpp.
//

#pragma once

#include "program.h"

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace alf::io
{
  /** One thread per performance core (all cores where the system doesn't say). */
  [[nodiscard]] auto default_threads() -> unsigned;

  /** Filter `infile` (or stdin) into `outfile` (or stdout); throws std::runtime_error on errors. */
  void filter(std::optional<std::string> const& infile, std::optional<std::string> const& outfile,
              alf::Program const& program, unsigned threads);

  /** Write `text` to stdout (the same path as filtered output). */
  void write_stdout(std::string_view text);

  /**
   * stdin or a pipe: read blocks with `read` (bytes read into the buffer, 0 at the end) into one
   * reused buffer, filter the complete lines, hand the kept ones to `out`, and keep only the
   * partial last line for the next block.  `out.flush` is called before the buffer is reused.
   */
  void filter_stream(std::function<std::size_t(char*, std::size_t)> const& read, alf::Program const& program,
                     unsigned threads, alf::Output const& out);
}
