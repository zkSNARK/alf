#include "parser.h"
#include "shunting_yard.h"
#include "line_filter.h"

#include <cstdio>
#include <exception>
#include <string>


auto apply_filters(alf::parser::ArgPack p) -> void {
  FILE *fi = p.infile ? fopen(p.infile->c_str(), "r") : stdin;
  FILE *fo = p.outfile ? fopen(p.outfile->c_str(), "w+") : stdout;

  if (fi == nullptr or fo == nullptr) {
    std::perror(fi == nullptr ? p.infile->c_str() : p.outfile->c_str());
    exit(EXIT_FAILURE);
  }

  char *line = nullptr;
  size_t cap = 0;
  ssize_t n;
  std::string text;
  while ((n = getline(&line, &cap, fi)) != -1) {
    // match against the line without its line ending, so a regex like r'done$' works
    size_t end = static_cast<size_t>(n);
    if (end > 0 and line[end - 1] == '\n') { --end; }
    if (end > 0 and line[end - 1] == '\r') { --end; }
    text.assign(line, end);
    if (alf::passes_filters(p.tokens, text)) {
      fwrite(line, 1, static_cast<size_t>(n), fo);
    }
  }
  fclose(fi);
  fclose(fo);

  if (line) {
    free(line);
  }
}


// SIGPIPE keeps its default action, so `alf ... | head` ends quietly once head has its lines.
int main(int argc, const char **argv) {
  try {
    alf::parser::ArgPack pack { alf::parser::parse_arguments(argc, argv)};
    pack.tokens = alf::parser::fill_in_missing_AND_symbols(pack.tokens);
    pack.tokens = alf::shunting_yard(std::move(pack.tokens));
    (void) alf::passes_filters(pack.tokens, "");  // reject a malformed expression before reading any input
    apply_filters(std::move(pack));
  } catch (std::exception const& e) {
    std::fprintf(stderr, "alf: %s\n", e.what());
    return 2;
  }

  return 0;
}
