//
// io_posix.cpp -- input and output on Linux, macOS and other POSIX systems (and Emscripten).
//
// Regular files are memory-mapped and kept lines are written straight from the mapping with
// writev; stdin and pipes are read in blocks (io_stream.cpp).
//

#include "io.h"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstring>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#if defined(__APPLE__)
#include <sys/sysctl.h>
#endif
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>

namespace
{
  auto error(std::string const& what) -> std::runtime_error
  {
    return std::runtime_error(what + ": " + std::strerror(errno));
  }

  // Block until `fd` can take more output (it was opened non-blocking by someone else: a pipe
  // set up by a parent process, or stdout under Node.js).
  void wait_writable(int fd)
  {
    pollfd p{ fd, POLLOUT, 0 };
    while (poll(&p, 1, -1) < 0 and errno == EINTR) {
    }
  }

  /**
   * Writes views straight from the input's memory with writev: no stdio buffer, no copy in this
   * process.  Views are batched and must stay valid until flush(): filter() calls it before
   * releasing its own buffers, and the callers below call it before unmapping or reusing the input.
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
          if (errno == EAGAIN or errno == EWOULDBLOCK) {  // a non-blocking output that's full
            wait_writable(m_fd);
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

    [[nodiscard]] auto output() -> alf::Output
    {
      return { [this](std::string_view v) { add(v); }, [this] { flush(); } };
    }

  private:
    static constexpr std::size_t BATCH = IOV_MAX < 1024 ? IOV_MAX : 1024;
    int m_fd;
    std::vector<iovec> m_iov;
  };

  // A regular file: map it, filter it, write the kept lines straight from the mapping.
  void filter_mapped(int fd, std::size_t size, alf::Program const& program, unsigned threads, Writer& out)
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
    alf::filter(std::string_view(static_cast<char const*>(m), size), program, threads, out.output());
    out.flush();  // every view points into the mapping: write them all before unmapping
    munmap(m, size);
  }
}


// One thread per performance core.  On a chip with slower efficiency cores (Apple silicon),
// threads on those cores finish last and the whole filter waits for them: measured on an M4 Max,
// 12 threads were as fast as 16 or faster on every benchmark task and used 10-20% less CPU.
// Where the system doesn't say which cores are which, use them all.
auto alf::io::default_threads() -> unsigned
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


void alf::io::filter(std::optional<std::string> const& infile, std::optional<std::string> const& outfile,
                     alf::Program const& program, unsigned threads)
{
  int in = infile ? open(infile->c_str(), O_RDONLY) : STDIN_FILENO;
  if (in < 0) {
    throw error(*infile);
  }
  int outfd = outfile ? open(outfile->c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644) : STDOUT_FILENO;
  if (outfd < 0) {
    throw error(*outfile);
  }
  Writer out{ outfd };

  struct stat sb{};
  if (fstat(in, &sb) == 0 and S_ISREG(sb.st_mode)) {
    filter_mapped(in, static_cast<std::size_t>(sb.st_size), program, threads, out);
  } else {
    filter_stream([in](char* buf, std::size_t n) -> std::size_t {
      for (;;) {
        ssize_t got = read(in, buf, n);
        if (got >= 0) {
          return static_cast<std::size_t>(got);
        }
        if (errno != EINTR) {
          throw error("read failed");
        }
      }
    }, program, threads, out.output());
  }
  if (infile) {
    close(in);
  }
  if (outfile and close(outfd) != 0) {
    throw error("write failed");
  }
}


void alf::io::write_stdout(std::string_view text)
{
  Writer out{ STDOUT_FILENO };
  out.add(text);
  out.flush();
}
