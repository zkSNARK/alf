#include "parser.h"
#include "program.h"
#include "shunting_yard.h"
#include "../libalf/src/simd/simd.h"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstring>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <fcntl.h>
#if defined(__APPLE__)
#include <sys/sysctl.h>
#endif
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>

namespace
{
  // stdin and pipes are read in blocks of this much per thread
  constexpr std::size_t BLOCK_PER_THREAD = 4 << 20;

  // One thread per performance core.  On a chip with slower efficiency cores (Apple silicon),
  // threads on those cores finish last and the whole filter waits for them: measured on an M4
  // Max, 12 threads were as fast as 16 or faster on every benchmark task and used 10-20% less
  // CPU.  Where the system doesn't say which cores are which, use them all.
  auto default_threads() -> unsigned
  {
#if defined(__APPLE__)
    int perf = 0;
    std::size_t len = sizeof perf;
    if (sysctlbyname("hw.perflevel0.logicalcpu", &perf, &len, nullptr, 0) == 0 and perf > 0) {
      return static_cast<unsigned>(perf);
    }
#endif
    return std::max(1u, std::thread::hardware_concurrency());
  }

  auto error(std::string const& what) -> std::runtime_error
  {
    return std::runtime_error(what + ": " + std::strerror(errno));
  }

  /**
   * Writes views straight from the input's memory with writev: no stdio buffer, no copy in this
   * process.  Views are batched and must stay valid until flush(): filter() calls it before
   * releasing its own buffers, and filter_* call it before unmapping or reusing the input.
   */
  class Writer
  {
  public:
    explicit Writer(int fd) : m_fd{ fd } { m_iov.reserve(BATCH); }

    void add(std::string_view v)
    {
      m_iov.push_back({ const_cast<char*>(v.data()), v.size() });
      if (m_iov.size() == BATCH) {
        flush();
      }
    }

    void flush()
    {
      iovec* iov = m_iov.data();
      int left = static_cast<int>(m_iov.size());
      while (left > 0) {
        ssize_t n = writev(m_fd, iov, left);
        if (n < 0) {
          if (errno == EINTR) {
            continue;
          }
          throw error("write failed");
        }
        // skip what was written; a partial write leaves the rest of one view for the next call
        auto done = static_cast<std::size_t>(n);
        while (left > 0 and done >= iov->iov_len) {
          done -= iov->iov_len;
          ++iov;
          --left;
        }
        if (left > 0) {
          iov->iov_base = static_cast<char*>(iov->iov_base) + done;
          iov->iov_len -= done;
        }
      }
      m_iov.clear();
    }

  private:
    static constexpr std::size_t BATCH = IOV_MAX < 1024 ? IOV_MAX : 1024;
    int m_fd;
    std::vector<iovec> m_iov;
  };

  // A regular file: map it, filter it, write the kept lines straight from the mapping.
  auto filter_mapped(int fd, std::size_t size, alf::Program const& program, unsigned threads, Writer& out) -> void
  {
    if (size == 0) {
      return;
    }
    void* m = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (m == MAP_FAILED) {
      throw error("mmap failed");
    }
    // Ask for the whole file up front.  Without this, every thread page-faults its way through
    // its chunk and on macOS they queue on the kernel's VM locks: measured on an M4 Max, the hint
    // makes a one-thread scan 40% faster and halves the CPU time of a 12-thread one.  (On Linux,
    // which already maps neighbouring pages on each fault, it is about neutral.)
    madvise(m, size, MADV_WILLNEED);
    alf::filter(std::string_view(static_cast<char const*>(m), size), program, threads,
                { [&](std::string_view v) { out.add(v); }, [&] { out.flush(); } });
    out.flush();  // every view points into the mapping: write them all before unmapping
    munmap(m, size);
  }

  // stdin or a pipe: read blocks into one reused buffer (never zero-filled), filter the complete
  // lines, write them from the buffer, and move only the partial last line to the front.
  auto filter_stream(int fd, alf::Program const& program, unsigned threads, Writer& out) -> void
  {
    std::size_t const block = BLOCK_PER_THREAD * threads;
    std::size_t cap = 2 * block;
    std::unique_ptr<char[]> buf(new char[cap]);  // default-initialized: no memset of the buffer
    std::size_t have = 0;
    alf::Output const write{ [&](std::string_view v) { out.add(v); }, [&] { out.flush(); } };
    for (;;) {
      if (cap - have < block) {  // one line longer than everything read so far: grow
        cap *= 2;
        std::unique_ptr<char[]> bigger(new char[cap]);
        std::memcpy(bigger.get(), buf.get(), have);
        buf = std::move(bigger);
      }
      std::size_t got = 0;
      while (got < block) {  // fill the block (a pipe hands over at most a pipe-buffer at a time)
        ssize_t n = read(fd, buf.get() + have + got, block - got);
        if (n < 0 and errno == EINTR) {
          continue;
        }
        if (n < 0) {
          throw error("read failed");
        }
        if (n == 0) {
          break;
        }
        got += static_cast<std::size_t>(n);
      }
      have += got;
      std::string_view const data(buf.get(), have);
      if (got == 0) {  // end of input: whatever is left is the last line
        alf::filter(data, program, threads, write);
        out.flush();
        return;
      }
      auto const last_nl = data.rfind('\n');
      if (last_nl == std::string_view::npos) {
        continue;  // no complete line yet
      }
      alf::filter(data.substr(0, last_nl + 1), program, threads, write);
      out.flush();  // the views point into buf, which is about to be reused
      have -= last_nl + 1;
      std::memmove(buf.get(), buf.get() + last_nl + 1, have);
    }
  }
}


auto apply_filters(alf::parser::ArgPack const& p, alf::Program const& program) -> void
{
  unsigned const threads = p.threads ? p.threads : default_threads();
  int in = p.infile ? open(p.infile->c_str(), O_RDONLY) : STDIN_FILENO;
  if (in < 0) {
    throw error(*p.infile);
  }
  int outfd = p.outfile ? open(p.outfile->c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644) : STDOUT_FILENO;
  if (outfd < 0) {
    throw error(*p.outfile);
  }
  Writer out{ outfd };

  struct stat sb{};
  if (fstat(in, &sb) == 0 and S_ISREG(sb.st_mode)) {
    filter_mapped(in, static_cast<std::size_t>(sb.st_size), program, threads, out);
  } else {
    filter_stream(in, program, threads, out);
  }
  if (p.infile) {
    close(in);
  }
  if (p.outfile and close(outfd) != 0) {
    throw error("write failed");
  }
}


// SIGPIPE keeps its default action, so `alf ... | head` ends quietly once head has its lines.
int main(int argc, const char **argv) {
  if (argc == 2 and std::string_view(argv[1]) == "--simd-info") {  // which SIMD backends this binary has
    for (alf::simd::Backend const* b : alf::simd::compiled()) {
      std::printf("%-7s %s%s\n", b->name, b->runs_here() ? "runs here" : "not on this CPU",
                  b == &alf::simd::active() ? "  (in use)" : "");
    }
    return 0;
  }
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
