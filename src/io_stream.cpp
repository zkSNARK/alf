//
// io_stream.cpp -- block reading for stdin and pipes, shared by every platform.
//

#include "io.h"

#include <cstring>
#include <memory>

namespace
{
  // stdin and pipes are read in blocks of this much per thread
  constexpr std::size_t BLOCK_PER_THREAD = 4 << 20;
}

void alf::io::filter_stream(std::function<std::size_t(char*, std::size_t)> const& read, alf::Program const& program,
                            unsigned threads, alf::Output const& out)
{
  std::size_t const block = BLOCK_PER_THREAD * threads;
  std::size_t cap = 2 * block;
  std::unique_ptr<char[]> buf(new char[cap]);  // default-initialized: no memset of the buffer
  std::size_t have = 0;
  for (;;) {
    if (cap - have < block) {  // one line longer than everything read so far: grow
      cap *= 2;
      std::unique_ptr<char[]> bigger(new char[cap]);
      std::memcpy(bigger.get(), buf.get(), have);
      buf = std::move(bigger);
    }
    std::size_t got = 0;
    while (got < block) {  // fill the block (a pipe hands over at most a pipe-buffer at a time)
      std::size_t const n = read(buf.get() + have + got, block - got);
      if (n == 0) {
        break;
      }
      got += n;
    }
    have += got;
    std::string_view const data(buf.get(), have);
    if (got == 0) {  // end of input: whatever is left is the last line
      alf::filter(data, program, threads, out);
      out.flush();
      return;
    }
    auto const last_nl = data.rfind('\n');
    if (last_nl == std::string_view::npos) {
      continue;  // no complete line yet
    }
    alf::filter(data.substr(0, last_nl + 1), program, threads, out);
    out.flush();  // the views point into buf, which is about to be reused
    have -= last_nl + 1;
    std::memmove(buf.get(), buf.get() + last_nl + 1, have);
  }
}
