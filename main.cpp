#include "global.h"
#include <cstdio>
#include <iostream>

int main() {
  srand(0);
  int i = 5;
  while (i++ < 1000) {
    std::size_t sz(((rand() % 100) + 1) * 4096);
    // std::size_t sz(i * 4096);
    printf("alloc: %zu\n", sz);
    void *ptr = global::alloc(sz);
    global::dealloc(ptr, sz);
  }
  // init();
  printf("test\n");
}
