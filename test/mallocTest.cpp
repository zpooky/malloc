#include "Util.h"
#include "malloc.h"
#include "shared.h"
#include "gtest/gtest.h"
#include <tuple>
#include <vector>

/*Test*/
TEST(MallocTest, test) {
  const size_t X = 1000;
  const size_t I = 1000;
  Points allocs;
  for (size_t x = 1; x < X; ++x) {
    for (size_t i = 1; i < I; ++i) {
      void *const ptr = sp_malloc(i);
      ASSERT_FALSE(ptr == nullptr);
      allocs.emplace_back(ptr, i);
      // sp_free(ptr);
      assert_no_overlap(allocs);
    }
  }
}

// TEST(MallocTest, test_calc_min_extent) {
//   std::size_t bucketSz = 8;
//   size_t idx = 0;
//   while (bucketSz > 0) {
//     std::size_t extSz = calc_min_extent2(bucketSz);
//
//     std::size_t idxs = (extSz - HEADER_SIZE) / bucketSz;
//     // printf("%zu. bucket:%zu|ext:%zu|idxs:%zu\n", //
//     //        idx++, bucketSz, extSz, idxs);
//     bucketSz = bucketSz << 1;
//   }
// }

// ./test/thetest --gtest_filter="*MallocTest*"
