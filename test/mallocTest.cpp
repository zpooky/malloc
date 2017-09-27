#include "Util.h"
#include "shared.h"
#include "gtest/gtest.h"
#include <malloc.h>
#include <malloc_debug.h>
#include <tuple>
#include <vector>

// TODO just alloc continuasly of same same
// TODO allocate increasing and wrap around
// TODO large range 1-XXX to see that we get the right bucket

/*Parametrized Fixture*/
class MallocRangeSizeTest : public testing::TestWithParam<size_t> {
public:
  MallocRangeSizeTest() {
  }

  virtual void SetUp() {
  }
  virtual void TearDown() {
  }
};

/*Setup Parameters*/
INSTANTIATE_TEST_CASE_P(Default, MallocRangeSizeTest,
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

class MallocTestAllocSizePFixture : public testing::TestWithParam<size_t> {
public:
  MallocTestAllocSizePFixture() {
  }

  virtual void SetUp() {
  }
  virtual void TearDown() {
  }
};

INSTANTIATE_TEST_CASE_P(Default, MallocTestAllocSizePFixture,
                        ::testing::Values(8, 16, 32, 64, 128, 256, 512, 1024,
                                          2048, 4096, 8192));

/*Test*/
static void malloc_test_uniform(std::size_t iterations, std::size_t allocSz) {
  Points allocs;
  allocs.reserve(iterations);

  // TODO assert 0 allocs of allocSz
  // ASSERT_EQ(0, debug::malloc_count_alloc());

  time("malloc", [&] { //
    for (size_t i = 1; i < iterations; ++i) {
      void *const ptr = sp_malloc(allocSz);
      ASSERT_FALSE(ptr == nullptr);
      allocs.emplace_back(ptr, allocSz);

      ASSERT_EQ(allocSz, sp_sizeof(ptr));
      ASSERT_EQ(ptr, sp_realloc(ptr, allocSz));
    }
  });
  // ASSERT_EQ(I - 1, debug::malloc_count_alloc());

  time("assert_no_overlap", [&] { //
    assert_no_overlap(allocs);
  });

  time("free", [&] { //
    for (auto c : allocs) {
      void *ptr = std::get<0>(c);
      ASSERT_EQ(std::get<1>(c), sp_sizeof(ptr));
      ASSERT_TRUE(sp_free(ptr));
    }
  });

  // ASSERT_EQ(0, debug::malloc_count_alloc());
}

const size_t NUMBER_OF_IT = /*16 * */ 1025;

TEST_P(MallocTestAllocSizePFixture, test_uniform) {
  const size_t allocSz = GetParam();
  malloc_test_uniform(NUMBER_OF_IT, allocSz);
}

static std::size_t roundAlloc(std::size_t sz) {
  for (std::size_t i(8); i < ~std::size_t(0); i <<= 1) {
    if (sz <= i) {
      // printf("sz(%zu)%zu\n", sz, i);
      return i;
    }
  }
  assert(false);
  return 0;
}

// static void range_realloc_same_size(void *ptr, std::size_t sz) {
  // for(rangeStart<rangeEnd){
  // ASSERT_EQ(ptr, sp_realloc(ptr, allocSz));
  // }
// }

TEST(MallocTest, test_range) {
  Points allocs;
  allocs.reserve(NUMBER_OF_IT);

  // TODO assert 0 allocs of allocSz

  time("malloc", [&] { //
    for (size_t i = 1; i < NUMBER_OF_IT; ++i) {
      const size_t allocSz = i;

      void *const ptr = sp_malloc(allocSz);
      ASSERT_FALSE(ptr == nullptr);
      std::size_t roundSz = roundAlloc(allocSz);
      allocs.emplace_back(ptr, roundSz);

      ASSERT_EQ(roundSz, sp_sizeof(ptr));
      ASSERT_EQ(ptr, sp_realloc(ptr, roundSz));
      ASSERT_EQ(ptr, sp_realloc(ptr, allocSz));
    }
  });

  // TODO assert I allocs of allocSz

  time("assert_no_overlap", [&] { //
    assert_no_overlap(allocs);
  });

  time("free", [&] { //
    for (auto c : allocs) {
      void *ptr = std::get<0>(c);
      ASSERT_EQ(std::get<1>(c), sp_sizeof(ptr));
      ASSERT_TRUE(sp_free(ptr));
    }
  });

  // TODO assert 0 allocs of allocSz
}

TEST(MallocTest, test_realloc) {
  std::vector<std::size_t> sizes{8,   16,   32,   64,   128, 256,
                                 512, 1024, 2048, 4096, 8192 /*, ... */};
  for (auto it = sizes.begin(); it != sizes.end(); ++it) {
    void *ptr = sp_malloc(*it);
    ASSERT_FALSE(ptr == nullptr);
    ASSERT_EQ(*it, sp_sizeof(ptr));

    for (auto rszIt = it + 1; rszIt != sizes.end(); ++rszIt) {
      void *nptr = sp_realloc(ptr, *rszIt);
      ASSERT_FALSE(ptr == nptr);
      ASSERT_EQ(*rszIt, sp_sizeof(nptr));
      ptr = nptr;
    }
    ASSERT_TRUE(sp_free(ptr));
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
