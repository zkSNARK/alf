#include "parser.h"
#include "shunting_yard.h"
#include "line_filter.h"

#include <string>
#include <csignal>

void handler(int) {
  printf("Caught SIGPIPE\n");
}


int main(int argc, const char **argv) {
  signal(SIGPIPE, handler);

  alf::parser::ArgPack pack { alf::parser::parse_arguments(argc, argv)};
  alf::parser::fill_in_missing_AND_symbols(pack.tokens);
  pack.tokens = alf::shunting_yard(std::move(pack.tokens));
//  apply_filters(std::move(arg_pack));

  return 0;
}
