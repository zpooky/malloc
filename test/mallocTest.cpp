#include "malloc.h"
#include "Util.h"
#include "shared.h"
#include "gtest/gtest.h"
#include <tuple>
#include <vector>

// TODO just alloc continuasly of same same
// TODO allocate increasing and wrap around
// TODO large range 1-XXX to see that we get the right bucket

/*Parametrized Fixture*/
class MallocTest : public testing::TestWithParam<size_t> {
public:
  MallocTest() {
  }

  virtual void SetUp() {
  }
  virtual void TearDown() {
  }
};

/*Setup Parameters*/
INSTANTIATE_TEST_CASE_P(Default, MallocTest,
                        ::testing::Values( //
                            64             //
                            ,
                            128 //
                            ,
                            256 //
                            ,
                            512 //
                            ,
                            1024 //
                            ,
                            2048 //
                            ,
                            4096 //
                            ,
                            8192 //
                            ,
                            16384 //
                            ,
                            16384 * 2 //
                            ,
                            16384 * 4 //
                            ,
                            16384 * 8 //
                            ));

/*Test*/
TEST_P(MallocTest, test) {
  const size_t I = GetParam();
  const size_t allocSz = 8;

  Points allocs;
  allocs.reserve(I);

  // TODO assert 0 allocs of allocSz

  for (size_t i = 1; i < I; ++i) {
    void *const ptr = sp_malloc(allocSz);
    ASSERT_FALSE(ptr == nullptr);
    allocs.emplace_back(ptr, allocSz);
    ASSERT_EQ(allocSz, sp_sizeof(ptr));
    ASSERT_EQ(ptr, sp_realloc(ptr, allocSz));
  }
  // TODO assert I allocs of allocSz

  assert_no_overlap(allocs);

  for (auto c : allocs) {
    void *ptr = std::get<0>(c);
    ASSERT_EQ(allocSz, sp_sizeof(ptr));
    ASSERT_TRUE(sp_free(ptr));
  }

  // TODO assert 0 allocs of allocSz
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
