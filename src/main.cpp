#include "io.h"
#include "parser.h"
#include "program.h"
#include "shunting_yard.h"
#include "../libalf/src/simd/simd.h"

#include <cstdio>
#include <exception>
#include <string>
#include <string_view>


// POSIX: SIGPIPE keeps its default action, so `alf ... | head` ends quietly once head has its
// lines (Windows: see io_windows.cpp).
int main(int argc, const char **argv) {
  if (argc == 2 and std::string_view(argv[1]) == "--simd-info") {  // which SIMD backends this binary has
    std::string text;
    for (alf::simd::Backend const* b : alf::simd::compiled()) {
      std::string name = b->name;
      name.resize(8, ' ');
      text += name + (b->runs_here() ? "runs here" : "not on this CPU") + (b == &alf::simd::active() ? "  (in use)" : "") + "\n";
    }
    alf::io::write_stdout(text);
    return 0;
  }
  try {
    alf::parser::ArgPack pack { alf::parser::parse_arguments(argc, argv)};
    pack.tokens = alf::parser::fill_in_missing_AND_symbols(pack.tokens);
    pack.tokens = alf::shunting_yard(std::move(pack.tokens));
    alf::Program const program{ pack.tokens };  // rejects a malformed expression before reading any input
    alf::io::filter(pack.infile, pack.outfile, program, pack.threads ? pack.threads : alf::io::default_threads());
  } catch (std::exception const& e) {
    std::fprintf(stderr, "alf: %s\n", e.what());
    return 2;
  }

  return 0;
}
