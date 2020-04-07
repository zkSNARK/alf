#include "parser.h"
#include "shunting_yard.h"
#include "line_filter.h"

#include <string>
#include <csignal>

void handler(int) {
  printf("Caught SIGPIPE\n");
}


auto apply_filters(alf::parser::ArgPack p) -> void {
  FILE *fi = p.infile ? fopen(p.infile->c_str(), "r") : stdin;
  FILE *fo = p.outfile ? fopen(p.outfile->c_str(), "w+") : stdout;

  if (fi == nullptr or fo == nullptr) {
    exit(EXIT_FAILURE);
  }

  char *line = nullptr;
  size_t len = 0;
  while ((getline(&line, &len, fi)) != -1) {
    if (alf::passes_filters(p.tokens, line)) {
      fputs(line, fo);
    }
  }
  fclose(fi);
  fclose(fo);

  if (line) {
    free(line);
  }
}


int main(int argc, const char **argv) {
  signal(SIGPIPE, handler);

  alf::parser::ArgPack pack { alf::parser::parse_arguments(argc, argv)};
  pack.tokens = alf::parser::fill_in_missing_AND_symbols(pack.tokens);
  pack.tokens = alf::shunting_yard(std::move(pack.tokens));
  apply_filters(std::move(pack));

  return 0;
}
