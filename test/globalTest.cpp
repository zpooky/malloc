#include "gtest/gtest.h"
#define SP_MALLOC_TEST_NO_ALLOC
#include <global.h>
#include <stdint.h>

/*Util*/

struct alignas(64) dummy {
  char b[64];
};
static_assert(64 == sizeof(dummy), "");
static_assert(64 == alignof(dummy), "");

static bool in_range(void *const base, size_t length, void *const e,
                     size_t e_length) {
  uintptr_t in = reinterpret_cast<uintptr_t>(e);
  uintptr_t inEnd = in + e_length;

  uintptr_t rangeStart = reinterpret_cast<uintptr_t>(base);
  uintptr_t rangeEnd = rangeStart + length;

  return (in >= rangeStart && in < rangeEnd) && (inEnd <= rangeEnd);
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

template <typename Points>
static void assert_no_overlap(Points &ptrs) {
  // std::sort(ptrs.begin(), ptrs.end());
  printf("assert_no_overlap: %zu\n", ptrs.size());
  auto current = ptrs.begin();
  while (current != ptrs.end()) {
    auto it = current;
    while (it++ != ptrs.end()) {
      if (in_range(*current, *it)) {
        printf("    current[%p, %zu]\n\t it[%p, %zu]\n",     //
               std::get<0>(*current), std::get<1>(*current), //
               std::get<0>(*it), std::get<1>(*it));
        ASSERT_TRUE(false);
      }
    }
    // printf("%p\n", std::get<0>(*current));
    current++;
  }
}

template <size_t SIZE>
static void dummy_dealloc_setup(uint8_t (*range)[SIZE], size_t bucketSz) {
  size_t offset = 0;
  size_t i = 0;
  for (; offset < SIZE; offset = ++i * bucketSz) {
    ASSERT_TRUE(offset + bucketSz <= SIZE);
    void *const start = (void *)(reinterpret_cast<uintptr_t>(range) + offset);
    // printf("dealloc[%p,%zu]\n", start, sz);
    global::dealloc(start, bucketSz);
  }
  printf("dummy_dealloc_setup: %zu\n", i);
}

template <size_t SIZE>
static void dummy_dealloc_random_setup(uint8_t (*range)[SIZE],
                                       size_t bucketSz) {
  std::vector<void *> points;
  size_t offset = 0;
  size_t i = 0;
  for (; offset < SIZE; offset = ++i * bucketSz) {
    ASSERT_TRUE(offset + bucketSz <= SIZE);
    void *const start = (void *)(reinterpret_cast<uintptr_t>(range) + offset);
    points.push_back(start);
  }
  std::random_shuffle(points.begin(), points.end());
  for (auto &start : points) {
    global::dealloc(start, bucketSz);
  }
}

template <size_t SIZE>
static void assert_dummy_dealloc(uint8_t (*range)[SIZE], size_t bucketSz) {
  auto free = test::watch_free();
  ASSERT_EQ(SIZE / bucketSz, free.size());
  for (auto &current : free) {
    void *const cptr = std::get<0>(current);
    size_t csz = std::get<1>(current);
    ASSERT_EQ(csz, bucketSz);
    if (!in_range(range, SIZE, cptr, csz)) {
      uintptr_t rangeStartPtr = reinterpret_cast<uintptr_t>(range);
      void *const rangeStart = reinterpret_cast<void *>(rangeStartPtr);
      void *const rangeEnd = reinterpret_cast<void *>(rangeStartPtr + SIZE);

      printf("range[%p-%p]", rangeStart, rangeEnd);
      printf("current[%p,%zu]\n", cptr, csz);
      ASSERT_TRUE(false);
    }
  }
  assert_no_overlap(free);
}

template <size_t SIZE>
static void assert_dummy_alloc(uint8_t (*range)[SIZE], size_t bucketSz) {
  for (size_t i = 0; i < SIZE; i += bucketSz) {
    void *const current = global::alloc(bucketSz);
    if (!in_range(range, SIZE, current, bucketSz)) {
      uintptr_t rangeStartPtr = reinterpret_cast<uintptr_t>(range);
      void *const rangeStart = reinterpret_cast<void *>(rangeStartPtr);
      void *const rangeEnd = reinterpret_cast<void *>(rangeStartPtr + SIZE);

      printf("range[%p-%p]", rangeStart, rangeEnd);
      printf("current[%p,%zu]\n", current, bucketSz);
      printf("rangeStart=%p\n", rangeStart);
      printf("rangeEnd=%p\n", rangeEnd);
      printf("curStart=%p\n", current);
      void *curEnd = (void *)(reinterpret_cast<uintptr_t>(current) + bucketSz);
      printf("curEnd=%p\n", curEnd);
      printf("rangeStart > curStart and curStart < rangeEnd and curEnd <= "
             "rangeEnd\n");
      printf("%p > %p and %p < %p and %p <= %p\n", rangeStart, current, current,
             rangeEnd, curEnd, rangeEnd);
      ASSERT_TRUE(false);
    }
  }
}

/*Gtest*/
/*Parametrized Fixture*/
class GlobalTest : public testing::TestWithParam<size_t> {
public:
  virtual void SetUp() {
    // ASSERT_EQ(size_t(0), test::watch_free().size());
  }
  virtual void TearDown() {
    // ASSERT_EQ(size_t(0), test::watch_free().size());
  }
};

/*Setup Parameters*/
INSTANTIATE_TEST_CASE_P(Default, GlobalTest,
                        ::testing::Values(64 //
                                          ,
                                          128, 256, 512, 1024, 2048, 4096, 8192,
                                          16384 //
                                          ));
/*Test*/
TEST_P(GlobalTest, dealloc_alloc_symmetric) {
  ASSERT_EQ(size_t(0), test::watch_free().size());
  const size_t sz = GetParam();

  const size_t SIZE = 1024;
  alignas(64) uint8_t range[SIZE * 64];
  dummy_dealloc_setup(&range, sz);

  assert_dummy_dealloc(&range, sz);

  assert_dummy_alloc(&range, sz);
  ASSERT_EQ(size_t(0), test::watch_free().size());
}

TEST_P(GlobalTest, dealloc_doubling_alloc) {
  ASSERT_EQ(size_t(0), test::watch_free().size());
  const size_t sz = GetParam();

  const size_t SIZE = 1024;
  alignas(64) uint8_t range[SIZE * 64];
  dummy_dealloc_setup(&range, sz);

  assert_dummy_dealloc(&range, sz);

  assert_dummy_alloc(&range, sz * 2);
  ASSERT_EQ(size_t(0), test::watch_free().size());
}

TEST_P(GlobalTest, dealloc_half_alloc) {
  ASSERT_EQ(size_t(0), test::watch_free().size());
  const size_t sz = GetParam();

  const size_t SIZE = 1024;
  alignas(64) uint8_t range[SIZE * 64];
  dummy_dealloc_setup(&range, sz);

  assert_dummy_dealloc(&range, sz);

  assert_dummy_alloc(&range, sz / 2);
  ASSERT_EQ(size_t(0), test::watch_free().size());
}

TEST_P(GlobalTest, dealloc_random_alloc) {
  ASSERT_EQ(size_t(0), test::watch_free().size());
  const size_t sz = GetParam();

  const size_t SIZE = 1024;
  alignas(64) uint8_t range[SIZE * 64];
  dummy_dealloc_random_setup(&range, sz);

  assert_dummy_dealloc(&range, sz);

  assert_dummy_alloc(&range, sz);
  ASSERT_EQ(size_t(0), test::watch_free().size());
}

// TODO threaded
