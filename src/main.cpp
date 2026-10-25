#include "parser.h"
#include "program.h"
#include "shunting_yard.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
  // stdin and pipes are read in blocks of this much per thread
  constexpr std::size_t BLOCK_PER_THREAD = 4 << 20;

  auto write_all(FILE* fo) -> std::function<void(std::string_view)>
  {
    return [fo](std::string_view s) {
      if (!s.empty() and std::fwrite(s.data(), 1, s.size(), fo) != s.size()) {
        throw std::runtime_error(std::string("write failed: ") + std::strerror(errno));
      }
    };
  }

  // A regular file: map it and filter it in one go.
  auto filter_mapped(int fd, std::size_t size, alf::Program const& program, unsigned threads, FILE* fo) -> void
  {
    if (size == 0) {
      return;
    }
    void* m = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (m == MAP_FAILED) {
      throw std::runtime_error(std::string("mmap failed: ") + std::strerror(errno));
    }
    madvise(m, size, MADV_SEQUENTIAL);
    alf::filter(std::string_view(static_cast<char const*>(m), size), program, threads, write_all(fo));
    munmap(m, size);
  }

  // stdin or a pipe: read large blocks, filter the complete lines, carry the partial last line.
  auto filter_stream(int fd, alf::Program const& program, unsigned threads, FILE* fo) -> void
  {
    std::size_t const block = BLOCK_PER_THREAD * threads;
    std::string buf;
    auto const write = write_all(fo);
    for (;;) {
      std::size_t const have = buf.size();
      buf.resize(have + block);
      std::size_t got = 0;
      while (got < block) {  // fill the block (a pipe hands over at most a pipe-buffer at a time)
        ssize_t n = read(fd, buf.data() + have + got, block - got);
        if (n < 0 and errno == EINTR) {
          continue;
        }
        if (n < 0) {
          throw std::runtime_error(std::string("read failed: ") + std::strerror(errno));
        }
        if (n == 0) {
          break;
        }
        got += static_cast<std::size_t>(n);
      }
      buf.resize(have + got);
      if (got == 0) {  // end of input: whatever is left is the last line
        alf::filter(buf, program, threads, write);
        return;
      }
      auto last_nl = buf.rfind('\n');
      if (last_nl == std::string::npos) {
        continue;  // no complete line yet
      }
      alf::filter(std::string_view(buf).substr(0, last_nl + 1), program, threads, write);
      buf.erase(0, last_nl + 1);
    }
  }
}


auto apply_filters(alf::parser::ArgPack const& p, alf::Program const& program) -> void
{
  unsigned const threads = p.threads ? p.threads : std::max(1u, std::thread::hardware_concurrency());
  int fd = p.infile ? open(p.infile->c_str(), O_RDONLY) : STDIN_FILENO;
  if (fd < 0) {
    throw std::runtime_error(*p.infile + ": " + std::strerror(errno));
  }
  FILE* fo = p.outfile ? std::fopen(p.outfile->c_str(), "w") : stdout;
  if (fo == nullptr) {
    throw std::runtime_error(*p.outfile + ": " + std::strerror(errno));
  }

  struct stat sb{};
  if (fstat(fd, &sb) == 0 and S_ISREG(sb.st_mode)) {
    filter_mapped(fd, static_cast<std::size_t>(sb.st_size), program, threads, fo);
  } else {
    filter_stream(fd, program, threads, fo);
  }
  if (p.infile) {
    close(fd);
  }
  if (std::fflush(fo) != 0) {
    throw std::runtime_error(std::string("write failed: ") + std::strerror(errno));
  }
  if (p.outfile) {
    std::fclose(fo);
  }
}


// SIGPIPE keeps its default action, so `alf ... | head` ends quietly once head has its lines.
int main(int argc, const char **argv) {
  try {
    alf::parser::ArgPack pack { alf::parser::parse_arguments(argc, argv)};
    pack.tokens = alf::parser::fill_in_missing_AND_symbols(pack.tokens);
    pack.tokens = alf::shunting_yard(std::move(pack.tokens));
    alf::Program const program{ pack.tokens };  // rejects a malformed expression before reading any input
    apply_filters(pack, program);
  } catch (std::exception const& e) {
    std::fprintf(stderr, "alf: %s\n", e.what());
    return 2;
  }

  return 0;
}
