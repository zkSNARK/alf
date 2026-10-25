//
// io_windows.cpp -- input and output on Windows.
//
// Regular files are mapped (CreateFileMapping / MapViewOfFile, with PrefetchVirtualMemory as the
// "I'll need all of this" hint); stdin and pipes are read in blocks (io_stream.cpp).
//
// Output: Windows has no gather-write for ordinary handles, so the zero-copy path is per piece:
// a long view is written straight from the input with one WriteFile, and short pieces are
// collected into one buffer and written together (one WriteFile per 100,000 tiny pieces would be
// far slower than copying them).  Bytes are written exactly as they are: no text-mode CRLF
// translation.
//

#include "io.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace
{
  auto error(std::string const& what) -> std::runtime_error
  {
    DWORD const code = GetLastError();
    char text[512] = {};
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0, text, sizeof text, nullptr);
    std::string message = text;
    while (!message.empty() and (message.back() == '\n' or message.back() == '\r' or message.back() == '.')) {
      message.pop_back();
    }
    return std::runtime_error(what + ": " + message);
  }

  auto widen(std::string const& utf8) -> std::wstring
  {
    int const n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n > 0 ? n - 1 : 0), L'\0');
    if (n > 1) {
      MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, w.data(), n);
    }
    return w;
  }

  class Writer
  {
  public:
    explicit Writer(HANDLE h) : m_h{ h } { m_buf.reserve(BUFFER); }

    void add(std::string_view v)
    {
      if (v.size() >= DIRECT) {  // long runs go straight from the input
        flush();
        write(v);
        return;
      }
      if (m_buf.size() + v.size() > BUFFER) {
        flush();
      }
      m_buf.append(v);  // copied now, so the view's lifetime no longer matters
    }

    void flush()
    {
      write(m_buf);
      m_buf.clear();
    }

    [[nodiscard]] auto output() -> alf::Output
    {
      return { [this](std::string_view v) { add(v); }, [this] { flush(); } };
    }

  private:
    static constexpr std::size_t DIRECT = 64 << 10;
    static constexpr std::size_t BUFFER = 1 << 20;

    void write(std::string_view v)
    {
      while (!v.empty()) {
        DWORD const chunk = static_cast<DWORD>(std::min<std::size_t>(v.size(), 1u << 30));
        DWORD done = 0;
        if (!WriteFile(m_h, v.data(), chunk, &done, nullptr)) {
          DWORD const code = GetLastError();
          if (code == ERROR_BROKEN_PIPE or code == ERROR_NO_DATA) {
            ExitProcess(0);  // the reader went away (`alf ... | more` quit): like SIGPIPE on POSIX
          }
          throw error("write failed");
        }
        v.remove_prefix(done);
      }
    }

    HANDLE m_h;
    std::string m_buf;
  };

  void filter_mapped(HANDLE file, std::size_t size, alf::Program const& program, unsigned threads, Writer& out)
  {
    if (size == 0) {
      return;
    }
    HANDLE const mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (mapping == nullptr) {
      throw error("CreateFileMapping failed");
    }
    void* view = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    if (view == nullptr) {
      CloseHandle(mapping);
      throw error("MapViewOfFile failed");
    }
    // the counterpart of MADV_WILLNEED (see io_posix.cpp): bring the whole file in up front
    WIN32_MEMORY_RANGE_ENTRY range{ view, size };
    PrefetchVirtualMemory(GetCurrentProcess(), 1, &range, 0);
    alf::filter(std::string_view(static_cast<char const*>(view), size), program, threads, out.output());
    out.flush();  // every view points into the mapping: write them all before unmapping
    UnmapViewOfFile(view);
    CloseHandle(mapping);
  }
}


// One thread per performance core: on CPUs with performance and efficiency cores, Windows gives
// the faster cores a higher EfficiencyClass.  Where all cores are alike, use them all.
auto alf::io::default_threads() -> unsigned
{
  DWORD len = 0;
  GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &len);
  std::vector<char> buf(len);
  auto* info = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buf.data());
  if (len == 0 or !GetLogicalProcessorInformationEx(RelationProcessorCore, info, &len)) {
    return std::max(1u, std::thread::hardware_concurrency());
  }
  BYTE best = 0;
  unsigned on_best = 0, all = 0;
  for (DWORD off = 0; off < len;) {
    auto const* p = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX const*>(buf.data() + off);
    unsigned logical = 0;
    for (WORD g = 0; g < p->Processor.GroupCount; ++g) {
      for (KAFFINITY m = p->Processor.GroupMask[g].Mask; m != 0; m &= m - 1) {
        ++logical;
      }
    }
    all += logical;
    if (p->Processor.EfficiencyClass > best) {
      best = p->Processor.EfficiencyClass;
      on_best = 0;
    }
    if (p->Processor.EfficiencyClass == best) {
      on_best += logical;
    }
    off += p->Size;
  }
  return std::max(1u, on_best ? on_best : all);
}


void alf::io::filter(std::optional<std::string> const& infile, std::optional<std::string> const& outfile,
                     alf::Program const& program, unsigned threads)
{
  HANDLE in = infile ? CreateFileW(widen(*infile).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                   OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr)
                     : GetStdHandle(STD_INPUT_HANDLE);
  if (in == INVALID_HANDLE_VALUE) {
    throw error(*infile);
  }
  HANDLE out_h = outfile ? CreateFileW(widen(*outfile).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                       FILE_ATTRIBUTE_NORMAL, nullptr)
                         : GetStdHandle(STD_OUTPUT_HANDLE);
  if (out_h == INVALID_HANDLE_VALUE) {
    throw error(*outfile);
  }
  Writer out{ out_h };

  LARGE_INTEGER size{};
  if (GetFileType(in) == FILE_TYPE_DISK and GetFileSizeEx(in, &size)) {
    filter_mapped(in, static_cast<std::size_t>(size.QuadPart), program, threads, out);
  } else {
    filter_stream([in](char* buf, std::size_t n) -> std::size_t {
      DWORD got = 0;
      if (!ReadFile(in, buf, static_cast<DWORD>(std::min<std::size_t>(n, 1u << 30)), &got, nullptr)) {
        if (GetLastError() == ERROR_BROKEN_PIPE) {
          return 0;  // the writer closed its end: end of input
        }
        throw error("read failed");
      }
      return got;
    }, program, threads, out.output());
  }
  out.flush();
  if (infile) {
    CloseHandle(in);
  }
  if (outfile) {
    CloseHandle(out_h);
  }
}


void alf::io::write_stdout(std::string_view text)
{
  Writer out{ GetStdHandle(STD_OUTPUT_HANDLE) };
  out.add(text);
  out.flush();
}
