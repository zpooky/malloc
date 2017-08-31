#include "global.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <tuple>
#include <vector>

template <typename Function>
void time(const char *msg, Function f) {
  auto t1 = std::chrono::high_resolution_clock::now();
  f();
  auto t2 = std::chrono::high_resolution_clock::now();
  std::cout
      << msg << "|Delta t2-t1: "
      << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count()
      << "ms" << std::endl;
}

template <typename C>
void check_overlap(C &ptrs) {
  std::sort(ptrs.begin(), ptrs.end());
  auto current = ptrs.begin();
  while (current != ptrs.end()) {
    // printf("%p\n", std::get<0>(*current));
    current++;
  }
}

int main() {
  srand(0);
  std::vector<std::tuple<void *, std::size_t>> ptrs;
  std::size_t alloc(0);

  // while (true) {
  time("alloc", [&]() {
    int i = 0;
    while (i++ < 10000) {
      std::size_t sz(((rand() % 100) + 1) * 4096);
      // std::size_t sz(i * 4096);
      // printf("%zu\n", sz);
      void *ptr = global::alloc(sz);
      assert(ptr);
      alloc += sz;
      ptrs.emplace_back(ptr, sz);
    }
  });
  printf("total: %zu\n", alloc);
  check_overlap(ptrs);

  time("dealloc", [&]() {
    for (auto &l : ptrs) {
      //   printf("alloc: %zu\n", std::get<1>(l));
      void *const ptr = std::get<0>(l);
      std::size_t len = std::get<1>(l);
      global::dealloc(ptr, len);
    }
  });

  time("alloc2", [&]() {
    int i = 0;
    while (i++ < 10000) {
      std::size_t sz(((rand() % 100) + 1) * 4096);
      // std::size_t sz(i * 4096);
      void *const ptr = global::alloc(sz);
      global::dealloc(ptr, sz);
    }
  });
  // }
  printf("done\n");
}
