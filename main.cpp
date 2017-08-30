#include "global.h"
#include <chrono>
#include <cstdio>
#include <iostream>
#include <tuple>
#include <vector>

int main() {
  srand(0);
  int i = 5;
  std::vector<std::tuple<void *, std::size_t>> ptrs;

  auto t1 = std::chrono::high_resolution_clock::now();
  while (i++ < 10000) {
    std::size_t sz(((rand() % 100) + 1) * 4096);
    // std::size_t sz(i * 4096);
    void *ptr = global::alloc(sz);
    ptrs.emplace_back(ptr, sz);
  }
  auto t2 = std::chrono::high_resolution_clock::now();
  std::cout
      << "Delta t2-t1: "
      << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count()
      << "ms" << std::endl;
  // for (auto &l : ptrs) {
  //   printf("alloc: %zu\n", std::get<1>(l));
  //   global::dealloc(std::get<0>(l), std::get<1>(l));
  // }
  // init();
  printf("test\n");
}
