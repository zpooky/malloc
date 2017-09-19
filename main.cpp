#include "global.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <stdio.h>
#include <tuple>
#include <vector>

template <typename Function>
static void time(const char *msg, Function f) {
  auto t1 = std::chrono::high_resolution_clock::now();
  f();
  auto t2 = std::chrono::high_resolution_clock::now();
  std::cout
      << msg << "|Delta t2-t1: "
      << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count()
      << "ms" << std::endl;
}

template <typename T>
static bool in_range(T &b, T &e) {
  uintptr_t strtB = reinterpret_cast<uintptr_t>(std::get<0>(b));
  std::size_t lenB = std::get<1>(b);

  uintptr_t strtE = reinterpret_cast<uintptr_t>(std::get<0>(e));
  std::size_t lenE = std::get<1>(e);

  return (strtB >= strtE && strtB < (strtE + lenE)) ||
         (strtE >= strtB && strtE < (strtB + lenB));
}

template <typename C>
static void check_overlap(C &ptrs) {
  std::sort(ptrs.begin(), ptrs.end());
  auto current = ptrs.begin();
  while (current != ptrs.end()) {
    auto it = current;
    while (it++ != ptrs.end()) {
      if (in_range(*current, *it)) {
        printf("    current[%p, %zu]\n\t it[%p, %zu]\n",     //
               std::get<0>(*current), std::get<1>(*current), //
               std::get<0>(*it), std::get<1>(*it));
        assert(false);
      }
    }
    // printf("%p\n", std::get<0>(*current));
    current++;
  }
}

template <typename T>
T *pmath(T *v, ptrdiff_t diff) {
  uintptr_t result = reinterpret_cast<uintptr_t>(v);
  result = result + diff;
  return reinterpret_cast<T *>(result);
}

// template <typename Points>
// static void assert_consecutive_range(Points &free, Range range) {
//   // printf("assert_consecutive_range\n");
//   uint8_t *const startR = range.start;
//
//   auto cmp = [](const auto &first, const auto &second) -> bool {
//     return std::get<0>(first) < std::get<0>(second);
//   };
//   std::sort(free.begin(), free.end(), cmp);
//
//   void *first = startR;
//   for (auto it = free.begin(); it != free.end(); ++it) {
//     assert(std::get<0>(*it) = first);
//
//     first = pmath(std::get<0>(*it), std::get<1>(*it));
//   }
//   assert(first == pmath(range.start, +range.length));
// }

int main() {
  srand(0);

  while (true) {
    printf("==============================\n");

    std::vector<std::tuple<void *, std::size_t>> ptrs;
    std::size_t alloc(0);

    printf("free: %zu\n", test::count_free(nullptr));

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
    time("check_overlap", [&]() { //
      check_overlap(ptrs);
    });

    printf("free: %zu\n", test::count_free(nullptr));
    // assert(test::watch_free().size() == 0);

    time("dealloc", [&]() {
      for (auto &l : ptrs) {
        //   printf("alloc: %zu\n", std::get<1>(l));
        void *const ptr = std::get<0>(l);
        std::size_t len = std::get<1>(l);
        global::dealloc(ptr, len);
      }
    });
    printf("free: %zu\n", test::count_free(nullptr));

    time("alloc2", [&]() {
      int i = 0;
      while (i++ < 10000) {
        std::size_t sz(((rand() % 100) + 1) * 4096);
        // std::size_t sz(i * 4096);
        void *const ptr = global::alloc(sz);
        global::dealloc(ptr, sz);
      }
    });
    printf("free: %zu\n", test::count_free(nullptr));
    printf("done\n");
  }
}
