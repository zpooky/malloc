#include "malloc.h"
#include "gtest/gtest.h"
#include <tuple>
#include <vector>
#include "Util.h"

/*Test*/
TEST(MallocTest, test) {
  std::vector<std::tuple<void *, std::size_t>> allocs;
  for (size_t i = 1; i < 3; ++i) {
    void *const ptr = sp_malloc(i);
    allocs.emplace_back(ptr, i);
    // sp_free(ptr);
  }
}
// ./test/thetest --gtest_filter="*MallocTest*"
