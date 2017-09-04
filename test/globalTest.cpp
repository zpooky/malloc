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

  uintptr_t rangeStart = reinterpret_cast<uintptr_t>(base);
  uintptr_t rangeEnd = rangeStart + length;

  return (in >= rangeStart && in < rangeEnd) && in + length <= rangeEnd;
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
  std::sort(ptrs.begin(), ptrs.end());
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
static void dummy_dealloc_setup(uint8_t (&range)[SIZE], size_t sz) {
  size_t offset = 0;
  size_t i = 0;
  for (; offset < SIZE; offset = ++i * sz) {
    ASSERT_TRUE(offset + sz <= SIZE);
    void *const start = reinterpret_cast<void *>(range + offset);
    // printf("dealloc[%p,%zu]\n", start, sz);
    global::dealloc(start, sz);
  }
  printf("dummy_dealloc_setup: %zu\n", i);
}

template <size_t SIZE>
static void assert_dummy_dealloc(uint8_t (&range)[SIZE], size_t sz) {
  auto free = test::watch_free();
  ASSERT_EQ(SIZE / sz, free.size());
  for (auto &current : free) {
    if (!in_range(&range, sizeof(range), std::get<0>(current), sz)) {
      printf("range[%p,%zu]", &range, sizeof(range));
      printf("current[%p,%zu]\n", std::get<0>(current), std::get<1>(current));
      ASSERT_TRUE(false);
    }
  }
  assert_no_overlap(free);
}

template <size_t SIZE>
static void assert_dummy_alloc(uint8_t (&range)[SIZE], size_t sz) {
  for (size_t i = 0; i < SIZE; i += sz) {
    void *const current = global::alloc(sz);
    if (!in_range(&range, sizeof(range), current, sz)) {
      printf("range[%p,%zu]", &range, sizeof(range));
      printf("current[%p,%zu]\n", current, sz);
      ASSERT_TRUE(false);
    }
  }
}

/*Gtest*/
/*Parametrized Fixture*/
class GlobalTest : public testing::TestWithParam<size_t> {
public:
  virtual void SetUp() {
    ASSERT_EQ(size_t(0), test::watch_free().size());
  }
  virtual void TearDown() {
    ASSERT_EQ(size_t(0), test::watch_free().size());
  }
};

/*Setup Parameters*/
INSTANTIATE_TEST_CASE_P(Default, GlobalTest,
                        ::testing::Values(64, 128, 256, 512, 1024, 2048, 4096,
                                          8192, 16384));
/*Test*/
TEST_P(GlobalTest, dealloc_alloc_symmetric) {
  const size_t sz = GetParam();

  const size_t SIZE = 1024;
  alignas(64) uint8_t range[SIZE * 64];
  dummy_dealloc_setup(range, sz);

  assert_dummy_dealloc(range, sz);

  assert_dummy_alloc(range, sz);
}

// TEST_P(GlobalTest, dealloc_doubling_alloc) {
//   const size_t sz = GetParam();
//
//   const size_t SIZE = 1024;
//   alignas(64) uint8_t range[SIZE * 64];
//   dummy_dealloc_setup(range, sz);
//
//   assert_dummy_dealloc(range, sz);
//
//   assert_dummy_alloc(range, sz * 2);
// }
//
// TEST_P(GlobalTest, dealloc_half_alloc) {
//   const size_t sz = GetParam();
//
//   const size_t SIZE = 1024;
//   alignas(64) uint8_t range[SIZE * 64];
//   dummy_dealloc_setup(range, sz);
//
//   assert_dummy_dealloc(range, sz);
//
//   assert_dummy_alloc(range, sz / 2);
// }

// TODO random
// TODO threaded
