#include "libalf.h"

#include <string>
#include <csignal>

void handler(int) {
  printf("Caught SIGPIPE\n");
}


int main(int argc, const char **argv) {
  signal(SIGPIPE, handler);

  alf::ArgPack pack { alf::parse_arguments(argc, argv)};
//  apply_filters(std::move(arg_pack));

  return 0;
}
