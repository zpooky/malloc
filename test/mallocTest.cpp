#include "Barrier.h"
#include "Util.h"
#include "shared.h"
#include "gtest/gtest.h"
#include <malloc.h>
#include <malloc_debug.h>
#include <pthread.h>
#include <stuff_debug.h>
#include <tuple>
#include <vector>

// TODO allocate increasing and wrap around
// TODO large range 1-XXX to see that we get the right bucket

// TODO Cartesian product[allocSZ,iterations,threads[consumer,producer]]
// TODO mutliple producer multiple consumer (realloc+free+usable_size)
// TODO realloc from other thread then the malloc thread

// TODO think of fixeing the case when memory mapped to specific pool but
// the bucket is not currenctly reserved then "usuable_size" still returns
// its pool size when infact it should be 0 since it is not reserved?

//==================================================================================================
//==================================================================================================
/*Parametrized Fixture*/
class MallocRangeSizeTest : public testing::TestWithParam<size_t> {
public:
  MallocRangeSizeTest() {
  }

  virtual void
  SetUp() {
    // ASSERT_EQ(std::size_t(0), debug::stuff_count_unclaimed_pools());
  }
  virtual void
  TearDown() {
    ASSERT_EQ(std::size_t(0), debug::stuff_count_unclaimed_pools());
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
    ASSERT_EQ(std::size_t(0), debug::stuff_count_unclaimed_pools());
  }
  virtual void
  TearDown() {
    ASSERT_EQ(std::size_t(0), debug::stuff_count_unclaimed_pools());
  }
};

INSTANTIATE_TEST_CASE_P(DefaultInstance, MallocTestAllocSizePFixture,
                        ::testing::Values(8, 16, 32, 64, 128, 256, 512, 1024,
                                          2048, 4096, 8192));

// INSTANTIATE_TEST_CASE_P(AnotherInstance, MallocTestAllocSizePFixture,
//                         ::testing::Range(std::size_t(1), std::size_t(1026)));

const size_t NUMBER_OF_IT = /*16 **/ 1025;

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

      ASSERT_EQ(rndAllocSz, sp_usable_size(ptr));
      ASSERT_EQ(ptr, sp_realloc(ptr, allocSz));
    }
  });

  ASSERT_EQ(iterations, debug::malloc_count_alloc(rndAllocSz));
  ASSERT_EQ(iterations, debug::malloc_count_alloc());
  ASSERT_EQ(iterations, allocs.size());

  time("assert_no_overlap", [&] { //
    assert_no_overlap(allocs);
  });

  time("free", [&] { //
    for (auto c : allocs) {
      void *ptr = std::get<0>(c);
      ASSERT_EQ(std::get<1>(c), sp_usable_size(ptr));
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

TEST_P(MallocTestAllocSizePFixture, 1threads_test_uniform) {
  // create a thread and wait
  const size_t allocSz = GetParam();

  ThreadArg arg(NUMBER_OF_IT, allocSz);
  threads(1, arg, worker_malloc_test_uniform);
}

TEST_P(MallocTestAllocSizePFixture, 2threads_test_uniform) {
  // create a thread and wait
  const size_t allocSz = GetParam();

  ThreadArg arg(NUMBER_OF_IT, allocSz);
  threads(2, arg, worker_malloc_test_uniform);
}

TEST_P(MallocTestAllocSizePFixture, 3threads_test_uniform) {
  // create a thread and wait
  const size_t allocSz = GetParam();

  ThreadArg arg(NUMBER_OF_IT, allocSz);
  threads(3, arg, worker_malloc_test_uniform);
}

TEST_P(MallocTestAllocSizePFixture, 4threads_test_uniform) {
  // create a thread and wait
  const size_t allocSz = GetParam();

  ThreadArg arg(NUMBER_OF_IT, allocSz);
  threads(4, arg, worker_malloc_test_uniform);
}

TEST_P(MallocTestAllocSizePFixture, 5threads_test_uniform) {
  // create a thread and wait
  const size_t allocSz = GetParam();

  ThreadArg arg(NUMBER_OF_IT, allocSz);
  threads(5, arg, worker_malloc_test_uniform);
}

TEST_P(MallocTestAllocSizePFixture, 6threads_test_uniform) {
  // create a thread and wait
  const size_t allocSz = GetParam();

  ThreadArg arg(NUMBER_OF_IT, allocSz);
  threads(6, arg, worker_malloc_test_uniform);
}

TEST_P(MallocTestAllocSizePFixture, 7threads_test_uniform) {
  // create a thread and wait
  const size_t allocSz = GetParam();

  ThreadArg arg(NUMBER_OF_IT, allocSz);
  threads(7, arg, worker_malloc_test_uniform);
}

TEST_P(MallocTestAllocSizePFixture, 8threads_test_uniform) {
  // create a thread and wait
  const size_t allocSz = GetParam();

  ThreadArg arg(NUMBER_OF_IT, allocSz);
  threads(8, arg, worker_malloc_test_uniform);
}

//-----------------------------------------
/*
 * spawn X threads
 * main thread wait
 * in each thread malloc Y times of size Z
 * in each thread run serialized summarize ranges into vector shared between
 * threads
 * in each thread free its allocted memory
 * main thread check for overlapped memory allocations ranges
 */
static void
overlap_malloc_check(std::size_t iterations, std::size_t allocSz,
                     sp::Barrier *start, sp::Barrier *overlap_stop,
                     Points *result, sp::ReadWriteLock *lock) {
  Points allocs;
  allocs.reserve(iterations);

  start->await();

  const std::size_t rndAllocSz = roundAlloc(allocSz);
  ASSERT_EQ(std::size_t(0), debug::malloc_count_alloc());
  ASSERT_EQ(std::size_t(0), debug::malloc_count_alloc(rndAllocSz));

  time("malloc", [&] { //
    for (size_t i = 0; i < iterations; ++i) {
      ASSERT_EQ(i, debug::malloc_count_alloc());

      void *const ptr = sp_malloc(allocSz);
      ASSERT_FALSE(ptr == nullptr);
      allocs.emplace_back(ptr, rndAllocSz);

      ASSERT_EQ(rndAllocSz, sp_usable_size(ptr));
      ASSERT_EQ(ptr, sp_realloc(ptr, allocSz));
    }
  });

  ASSERT_EQ(iterations, debug::malloc_count_alloc(rndAllocSz));
  ASSERT_EQ(iterations, debug::malloc_count_alloc());
  ASSERT_EQ(iterations, allocs.size());

  {
    sp::EagerExclusiveLock guard{*lock};
    if (guard) {
      result->insert(result->end(), allocs.begin(), allocs.end());
    }
  }
  overlap_stop->await();

  time("free", [&] { //
    for (auto c : allocs) {
      void *ptr = std::get<0>(c);
      ASSERT_EQ(std::get<1>(c), sp_usable_size(ptr));
      ASSERT_TRUE(sp_free(ptr));
      ASSERT_FALSE(sp_free(ptr));
    }
  });

  ASSERT_EQ(std::size_t(0), debug::malloc_count_alloc());
}

template <typename T>
static void *
worker_overlap_malloc_check(void *arg) {
  T *a = (T *)arg;

  overlap_malloc_check(std::get<0>(*a), std::get<1>(*a), &std::get<2>(*a),
                       &std::get<3>(*a), &std::get<4>(*a), &std::get<5>(*a));
  return nullptr;
}

static void
test_overlap_malloc_check(std::size_t ths, std::size_t allocSz) {
  sp::Barrier initial(ths);
  sp::Barrier stop(ths);

  Points result;
  result.reserve(ths * NUMBER_OF_IT);

  sp::ReadWriteLock lock;

  using Arg = std::tuple<std::size_t, std::size_t, sp::Barrier &, sp::Barrier &,
                         Points &, sp::ReadWriteLock &>;

  Arg arg(NUMBER_OF_IT, allocSz, initial, stop, result, lock);
  threads(ths, arg, worker_overlap_malloc_check<decltype(arg)>);

  time("assert_no_overlap", [&] { //
    assert_no_overlap(result);
  });
}

TEST_P(MallocTestAllocSizePFixture, 2threads_overlap_malloc_check) {
  const size_t allocSz = GetParam();
  test_overlap_malloc_check(2, allocSz);
}

TEST_P(MallocTestAllocSizePFixture, 3threads_overlap_malloc_check) {
  const size_t allocSz = GetParam();
  test_overlap_malloc_check(3, allocSz);
}

TEST_P(MallocTestAllocSizePFixture, 4threads_overlap_malloc_check) {
  const size_t allocSz = GetParam();
  test_overlap_malloc_check(4, allocSz);
}

TEST_P(MallocTestAllocSizePFixture, 5threads_overlap_malloc_check) {
  const size_t allocSz = GetParam();
  test_overlap_malloc_check(5, allocSz);
}

TEST_P(MallocTestAllocSizePFixture, 6threads_overlap_malloc_check) {
  const size_t allocSz = GetParam();
  test_overlap_malloc_check(6, allocSz);
}

TEST_P(MallocTestAllocSizePFixture, 7threads_overlap_malloc_check) {
  const size_t allocSz = GetParam();
  test_overlap_malloc_check(7, allocSz);
}

TEST_P(MallocTestAllocSizePFixture, 8threads_overlap_malloc_check) {
  const size_t allocSz = GetParam();
  test_overlap_malloc_check(8, allocSz);
}
//-----------------------------------------
static void
malloc_random_free(std::size_t iterations, std::size_t allocSz) {
  Points allocs;
  allocs.reserve(iterations);

  const std::size_t rndAllocSz = roundAlloc(allocSz);
  ASSERT_EQ(std::size_t(0), debug::malloc_count_alloc());
  ASSERT_EQ(std::size_t(0), debug::malloc_count_alloc(rndAllocSz));

  std::size_t active = 0;
  time("malloc", [&] { //
    std::size_t i = 0;
    while (i < iterations) {

      for (std::size_t a = 0; a < 100; ++a, ++i) {
        ASSERT_EQ(active, debug::malloc_count_alloc());

        void *const ptr = sp_malloc(allocSz);
        ++active;

        ASSERT_FALSE(ptr == nullptr);
        allocs.emplace_back(ptr, rndAllocSz);

        ASSERT_EQ(rndAllocSz, sp_usable_size(ptr));
        ASSERT_EQ(ptr, sp_realloc(ptr, allocSz));
      }

      std::random_shuffle(allocs.begin(), allocs.end());

      for (std::size_t a = 0; a < 50; ++a) {
        auto c = allocs.back();
        allocs.pop_back();

        void *ptr = std::get<0>(c);
        ASSERT_EQ(std::get<1>(c), sp_usable_size(ptr));
        ASSERT_TRUE(sp_free(ptr));
        ASSERT_FALSE(sp_free(ptr));
        --active;
        ASSERT_EQ(active, debug::malloc_count_alloc());
      }
    }
  });

  ASSERT_EQ(active, debug::malloc_count_alloc());
  ASSERT_EQ(active, debug::malloc_count_alloc(rndAllocSz));
  ASSERT_EQ(active, allocs.size());

  time("free the rest", [&] { //
    for (auto c : allocs) {
      void *ptr = std::get<0>(c);
      ASSERT_EQ(std::get<1>(c), sp_usable_size(ptr));
      ASSERT_TRUE(sp_free(ptr));
      ASSERT_FALSE(sp_free(ptr));
      --active;
      ASSERT_EQ(active, debug::malloc_count_alloc());
    }
  });

  ASSERT_EQ(std::size_t(0), debug::malloc_count_alloc());
}

TEST_P(MallocTestAllocSizePFixture, test_malloc_random_free_repeating) {
  const size_t allocSz = GetParam();
  malloc_random_free(NUMBER_OF_IT, allocSz);
}

//-----------------------------------------

void
test_range(std::size_t it) {
  Points allocs;
  allocs.reserve(it);

  ASSERT_EQ(std::size_t(0), debug::malloc_count_alloc());

  time("malloc", [&] { //
    for (size_t i = 1; i <= it; ++i) {
      const size_t allocSz = i;

      void *const ptr = sp_malloc(allocSz);
      ASSERT_FALSE(ptr == nullptr);
      std::size_t roundSz = roundAlloc(allocSz);
      allocs.emplace_back(ptr, roundSz);

      ASSERT_EQ(roundSz, sp_usable_size(ptr));
      ASSERT_EQ(ptr, sp_realloc(ptr, roundSz));
      ASSERT_EQ(ptr, sp_realloc(ptr, allocSz));
    }
  });

  ASSERT_EQ(it, allocs.size());
  ASSERT_EQ(it, debug::malloc_count_alloc());

  time("assert_no_overlap", [&] { //
    assert_no_overlap(allocs);
  });

  time("free", [&] { //
    for (auto c : allocs) {
      void *ptr = std::get<0>(c);
      ASSERT_FALSE(ptr == nullptr);

      ASSERT_EQ(std::get<1>(c), sp_usable_size(ptr));
      ASSERT_TRUE(sp_free(ptr));
      ASSERT_FALSE(sp_free(ptr));
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
produce_malloc_result(std::size_t iterations, std::size_t allocSz,
                      Points &result) {
  ASSERT_EQ(std::size_t(0), debug::malloc_count_alloc());
  ASSERT_EQ(std::size_t(0), debug::malloc_count_alloc(allocSz));

  time("malloc", [&] { //
    for (size_t i = 0; i < iterations; ++i) {
      ASSERT_EQ(i, debug::malloc_count_alloc());
      ASSERT_EQ(i, debug::malloc_count_alloc(allocSz));

      void *const ptr = sp_malloc(allocSz);
      ASSERT_FALSE(ptr == nullptr);
      std::size_t roundSz = roundAlloc(allocSz);
      result.emplace_back(ptr, roundSz);

      ASSERT_EQ(roundSz, sp_usable_size(ptr));
      ASSERT_EQ(ptr, sp_realloc(ptr, roundSz));
      ASSERT_EQ(ptr, sp_realloc(ptr, allocSz));
    }
  });

  ASSERT_TRUE(result.size() >= iterations);
  std::atomic_thread_fence(std::memory_order_release);
}

template <typename T>
static void *
worker_produce_malloc_result(void *arg) {
  T *a = (T *)arg;
  produce_malloc_result(std::get<0>(*a), std::get<1>(*a), std::get<2>(*a));
  return nullptr;
}

TEST_P(MallocTestAllocSizePFixture, test_producer_die_main_dealloc) {
  const size_t allocSz = GetParam();
  Points result;
  std::size_t passes = 1;

  using Arg = std::tuple<std::size_t, std::size_t, Points &>;
  Arg arg(NUMBER_OF_IT, allocSz, result);

  for (std::size_t pass(0); pass < passes; ++pass) {
    ASSERT_TRUE(result.size() == pass * NUMBER_OF_IT);
    ASSERT_EQ(std::size_t(0), debug::malloc_count_alloc());

    threads(1, arg, worker_produce_malloc_result<Arg>);
    std::atomic_thread_fence(std::memory_order_acquire);
  }

  ASSERT_EQ(result.size(), passes * NUMBER_OF_IT);

  time("assert_no_overlap", [&] { //
    assert_no_overlap(result);
  });

  time("shuffle", [&] { //
    std::random_shuffle(result.begin(), result.end());
  });

  time("free", [&] { //
    for (auto c : result) {
      void *ptr = std::get<0>(c);
      std::size_t actualSz = std::get<1>(c);
      ASSERT_FALSE(ptr == nullptr);

      ASSERT_EQ(actualSz, sp_usable_size(ptr));
      ASSERT_EQ(ptr, sp_realloc(ptr, actualSz));

      ASSERT_TRUE(sp_free(ptr));
      ASSERT_FALSE(sp_free(ptr));
    }
  });
}

//-----------------------------------------
static void
test_realloc() {
  std::vector<std::size_t> sizes{8,   16,   32,   64,   128, 256,
                                 512, 1024, 2048, 4096, 8192 /*, ... */};

  ASSERT_EQ(std::size_t(0), debug::malloc_count_alloc());

  for (auto it = sizes.begin(); it != sizes.end(); ++it) {
    void *ptr = sp_malloc(*it);
    ASSERT_FALSE(ptr == nullptr);
    ASSERT_EQ(*it, sp_usable_size(ptr));

    for (auto rszIt = it + 1; rszIt != sizes.end(); ++rszIt) {
      ASSERT_EQ(std::size_t(1), debug::malloc_count_alloc());
      void *const nptr = sp_realloc(ptr, *rszIt);

      ASSERT_FALSE(nptr == nullptr);
      ASSERT_FALSE(ptr == nptr);

      // ASSERT_EQ(std::size_t(0), sp_usable_size(ptr));
      ASSERT_EQ(*rszIt, sp_usable_size(nptr));

      ptr = nptr;
    }

    ASSERT_EQ(std::size_t(1), debug::malloc_count_alloc());
    ASSERT_TRUE(sp_free(ptr));
    ASSERT_EQ(std::size_t(0), debug::malloc_count_alloc());
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

// ./test/thetest --gtest_filter="*MallocTest*"
