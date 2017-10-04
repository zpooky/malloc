#include "Util.h"
#include "shared.h"
#include "gtest/gtest.h"
#include <malloc.h>
#include <malloc_debug.h>
#include <pthread.h>
#include <tuple>
#include <vector>

// TODO just allocate continuously of same same
// TODO allocate increasing and wrap around
// TODO large range 1-XXX to see that we get the right bucket
//
// TODO Cartesian product[allocSZ,iterations,threads[consumer,producer]]

//==================================================================================================
//==================================================================================================
/*Parametrized Fixture*/
class MallocRangeSizeTest : public testing::TestWithParam<size_t> {
public:
  MallocRangeSizeTest() {
  }

  virtual void
  SetUp() {
  }
  virtual void
  TearDown() {
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

  virtual void
  SetUp() {
  }
  virtual void
  TearDown() {
  }
};

INSTANTIATE_TEST_CASE_P(DefaultInstance, MallocTestAllocSizePFixture,
                        ::testing::Values(8, 16, 32, 64, 128, 256, 512, 1024,
                                          2048, 4096, 8192));

// INSTANTIATE_TEST_CASE_P(AnotherInstance, MallocTestAllocSizePFixture,
//                         ::testing::Range(std::size_t(1), std::size_t(1026)));

//==================================================================================================
//==================================================================================================
/*util*/
static std::size_t
roundAlloc(std::size_t sz) {
  for (std::size_t i(8); i < ~std::size_t(0); i <<= 1) {
    if (sz <= i) {
      // printf("sz(%zu)%zu\n", sz, i);
      return i;
    }
  }
  assert(false);
  return 0;
}

template <typename Argument>
static void
threads(std::size_t threads, Argument &arg, void *(*worker)(void *)) {
  std::vector<pthread_t> tids;
  for (std::size_t i(0); i < threads; ++i) {
    pthread_t tid = 0;
    int ret = pthread_create(&tid, nullptr, worker, &arg);
    ASSERT_EQ(0, ret);
    ASSERT_FALSE(tid == 0);
    tids.push_back(tid);
  }

  for (auto tid : tids) {
    int ret = pthread_join(tid, nullptr);
    ASSERT_EQ(0, ret);
  }
}

//==================================================================================================
//==================================================================================================
/*Test*/
static void
malloc_test_uniform(std::size_t iterations, std::size_t allocSz) {
  Points allocs;
  allocs.reserve(iterations);

  const std::size_t rndAllocSz = roundAlloc(allocSz);
  ASSERT_EQ(std::size_t(0), debug::malloc_count_alloc());
  ASSERT_EQ(std::size_t(0), debug::malloc_count_alloc(rndAllocSz));

  time("malloc", [&] { //
    for (size_t i = 0; i < iterations; ++i) {
      ASSERT_EQ(i, debug::malloc_count_alloc());

      void *const ptr = sp_malloc(allocSz);
      ASSERT_FALSE(ptr == nullptr);
      allocs.emplace_back(ptr, rndAllocSz);

      ASSERT_EQ(rndAllocSz, sp_sizeof(ptr));
      ASSERT_EQ(ptr, sp_realloc(ptr, allocSz));
    }
  });

  ASSERT_EQ(iterations, debug::malloc_count_alloc(rndAllocSz));
  ASSERT_EQ(iterations, debug::malloc_count_alloc());

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

  ASSERT_EQ(std::size_t(0), debug::malloc_count_alloc());
}
using ThreadArg = std::tuple<std::size_t, std::size_t>;

static void *
worker_malloc_test_uniform(void *argument) {
  auto arg = (ThreadArg *)argument;
  std::size_t it = std::get<0>(*arg);
  std::size_t allocSz = std::get<1>(*arg);
  malloc_test_uniform(it, allocSz);
  return nullptr;
}

const size_t NUMBER_OF_IT = 16 * 1025;

TEST_P(MallocTestAllocSizePFixture, test_uniform) {
  // create a thread and wait
  const size_t allocSz = GetParam();

  ThreadArg arg(NUMBER_OF_IT, allocSz);
  threads(1, arg, worker_malloc_test_uniform);
}

//-----------------------------------------

void
test_range(std::size_t it) {
  Points allocs;
  allocs.reserve(it);

  ASSERT_EQ(std::size_t(0), debug::malloc_count_alloc());

  time("malloc", [&] { //
    for (size_t i = 1; i < it; ++i) {
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

  // TODO assert allocs

  time("assert_no_overlap", [&] { //
    assert_no_overlap(allocs);
  });

  time("free", [&] { //
    for (auto c : allocs) {
      void *ptr = std::get<0>(c);
      ASSERT_FALSE(ptr == nullptr);

      ASSERT_EQ(std::get<1>(c), sp_sizeof(ptr));
      ASSERT_TRUE(sp_free(ptr));
    }
  });

  ASSERT_EQ(std::size_t(0), debug::malloc_count_alloc());
}

static void *
worker_test_range(void *arg) {
  std::size_t *it = (std::size_t *)arg;
  test_range(*it);
  return nullptr;
}

TEST(MallocTest, test_range) {
  std::size_t it = NUMBER_OF_IT;
  threads(1, it, worker_test_range);
}

//-----------------------------------------
static void
test_realloc() {
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

static void *
worker_test_realloc(void *) {
  test_realloc();
  return nullptr;
}

TEST(MallocTest, test_realloc) {
  void *arg = nullptr;
  threads(1, arg, worker_test_realloc);
}
//-----------------------------------------

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
