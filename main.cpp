#include <algorithm>
#include <random>
// #include "dyn_tree.h"
#include "global.h"
#include "global_debug.h"
#include "tree/bst.h"
#include "tree/static_tree.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <stdio.h>
#include <string>
#include <tuple>
#include <vector>

template <typename Function>
static void
time(const char *msg, Function f) {
  auto t1 = std::chrono::high_resolution_clock::now();
  f();
  auto t2 = std::chrono::high_resolution_clock::now();
  std::cout
      << msg << "|Delta t2-t1: "
      << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count()
      << "ms" << std::endl;
}

template <typename T>
static bool
in_range(T &b, T &e) {
  uintptr_t strtB = reinterpret_cast<uintptr_t>(std::get<0>(b));
  std::size_t lenB = std::get<1>(b);

  uintptr_t strtE = reinterpret_cast<uintptr_t>(std::get<0>(e));
  std::size_t lenE = std::get<1>(e);

  return (strtB >= strtE && strtB < (strtE + lenE)) ||
         (strtE >= strtB && strtE < (strtB + lenB));
}

template <typename C>
static void
check_overlap(C &ptrs) {
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
T *
pmath(T *v, ptrdiff_t diff) {
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
//
//
