#include "shunting_yard.h"

#include <iostream>
#include <vector>
#include <queue>
#include <string>
#include <stack>



int main() {
  std::vector<std::string> v{"{", "re", "|", "abc", "}", "&", "(", "[", "par", "|", "m", "]", "&", "z", ")"};

  std::queue<std::string> que {shunting_yard(std::move(v))};


  while (!que.empty()) {
    std::cout << " " << que.front();
    que.pop();
  }
  std::cout << '\n';

  return 0;
}