#include "gtest/gtest.h"
#include <global.h>
#include <stdint.h>

/*Util*/
static bool in_range(void *r, size_t rSz, void *e, size_t eLen) {
  uintptr_t in = reinterpret_cast<uintptr_t>(e);
  uintptr_t inEnd = in + eLen;

  uintptr_t rangeStart = reinterpret_cast<uintptr_t>(r);
  uintptr_t rangeEnd = rangeStart + rSz;

  return (in >= rangeStart && in < rangeEnd) && (inEnd <= rangeEnd);
}

static size_t free_entries() {
  auto free = test::watch_free();
  return free.size();
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
  // printf("assert_no_overlap: %zu\n", ptrs.size());
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

static void dummy_dealloc_setup(uint8_t *const range, size_t rSz, size_t bSz) {
  size_t i = 0;
// printf("valid: ");
start:
  size_t offset = i * bSz;
  if (offset + bSz <= rSz) {
    i++;
    void *const start = (void *)(reinterpret_cast<uintptr_t>(range) + offset);

    global::dealloc(start, bSz);

    // printf("[%p,%zu]", start, bucketSz);
    goto start;
  }
  // printf("\n");
  // test::print_free();
  // printf("dummy_dealloc_setup: %zu\n", i);
}

static void dummy_dealloc_random_setup(uint8_t *const range, size_t rSz,
                                       size_t bSz) {
  std::vector<void *> points;
  size_t i = 0;
// printf("valid: ");
start:
  size_t offset = i * bSz;
  if (offset + bSz <= rSz) {
    i++;
    void *const start = (void *)(reinterpret_cast<uintptr_t>(range) + offset);

    points.push_back(start);

    // printf("[%p,%zu]", start, bucketSz);
    goto start;
  }

  std::random_shuffle(points.begin(), points.end());
  for (auto &start : points) {
    global::dealloc(start, bSz);
  }
}

static void assert_dummy_dealloc(uint8_t *const range, size_t rSz, size_t bSz) {
  auto free = test::watch_free();

  // 1.
  assert_no_overlap(free);

  // 2.
  ASSERT_EQ(rSz / bSz, free.size());

  // 3.
  for (auto &current : free) {
    void *const cur_ptr = std::get<0>(current);
    size_t curSz = std::get<1>(current);
    ASSERT_EQ(curSz, bSz);

    if (!in_range(range, rSz, cur_ptr, curSz)) {
      // uintptr_t rangeStartPtr = reinterpret_cast<uintptr_t>(range);
      // void *const rangeStart = reinterpret_cast<void *>(rangeStartPtr);
      // void *const rangeEnd = reinterpret_cast<void *>(rangeStartPtr + SIZE);
      //
      // printf("range[%p-%p]", rangeStart, rangeEnd);
      // printf("current[%p,%zu]\n", cptr, csz);
      ASSERT_TRUE(false);
    }
  }
}

static void assert_dummy_alloc(uint8_t *const range, size_t rSz, size_t bSz) {
  for (size_t i = 0; i < rSz; i += bSz) {

    void *const current = global::alloc(bSz);
    ASSERT_FALSE(current == nullptr);

    if (!in_range(range, rSz, current, bSz)) {
      // uintptr_t rangeStartPtr = reinterpret_cast<uintptr_t>(range);
      // void *const rangeStart = reinterpret_cast<void *>(rangeStartPtr);
      // void *const rangeEnd = reinterpret_cast<void *>(rangeStartPtr + SIZE);
      //
      // printf("range[%p-%p]", rangeStart, rangeEnd);
      // printf("current[%p,%zu]\n", current, bucketSz);
      // printf("rangeStart=%p\n", rangeStart);
      // printf("rangeEnd=%p\n", rangeEnd);
      // printf("curStart=%p\n", current);
      // void *curEnd = (void *)(reinterpret_cast<uintptr_t>(current) +
      // bucketSz);
      // printf("curEnd=%p\n", curEnd);
      // printf("rangeStart > curStart and curStart < rangeEnd and curEnd <= "
      //        "rangeEnd\n");
      // printf("%p > %p and %p < %p and %p <= %p\n", rangeStart, current,
      // current,
      //        rangeEnd, curEnd, rangeEnd);
      ASSERT_TRUE(false);
    }
  }
}

/*Gtest*/
/*Parametrized Fixture*/
class GlobalTest : public testing::TestWithParam<size_t> {
public:
  virtual void SetUp() {
    ASSERT_EQ(size_t(0), free_entries());
  }
  virtual void TearDown() {
    ASSERT_EQ(size_t(0), free_entries());
  }
};

/*Setup Parameters*/
INSTANTIATE_TEST_CASE_P(Default, GlobalTest,
                        ::testing::Values( //
                            64             //
                            ,
                            128, 256, 512, 1024, 2048, 4096, 8192, //
                            16384                                  //
                            ));
/*Test*/
TEST_P(GlobalTest, dealloc_alloc_symmetric) {
  // ASSERT_EQ(size_t(0), free_entries());
  const size_t sz = GetParam();

  const size_t SIZE = 1024 * 64;
  // alignas(64) uint8_t range[SIZE];
  uint8_t *const range = (uint8_t *)aligned_alloc(64, SIZE * 2);
  memset(range, 0, SIZE * 2);

  dummy_dealloc_setup(range, SIZE, sz);
  for (size_t i(SIZE); i < SIZE * 2; ++i) {
    ASSERT_EQ(range[i], uint8_t(0));
  }

  assert_dummy_dealloc(range, SIZE, sz);
  for (size_t i(SIZE); i < SIZE * 2; ++i) {
    ASSERT_EQ(range[i], uint8_t(0));
  }

  assert_dummy_alloc(range, SIZE, sz);
  for (size_t i(SIZE); i < SIZE * 2; ++i) {
    ASSERT_EQ(range[i], uint8_t(0));
  }

  ASSERT_EQ(size_t(0), free_entries());
  free(range);
}

// TEST_P(GlobalTest, dealloc_doubling_alloc) {
//   ASSERT_EQ(size_t(0), free_entries());
//   const size_t sz = GetParam();
//
//   const size_t SIZE = 1024;
//   alignas(64) uint8_t range[SIZE * 64];
//   dummy_dealloc_setup(&range, sz);
//
//   assert_dummy_dealloc(&range, sz);
//
//   assert_dummy_alloc(&range, sz * 2);
//   ASSERT_EQ(size_t(0), free_entries());
// }
//
// TEST_P(GlobalTest, dealloc_half_alloc) {
//   ASSERT_EQ(size_t(0), free_entries());
//   const size_t sz = GetParam();
//
//   const size_t SIZE = 1024;
//   alignas(64) uint8_t range[SIZE * 64];
//   dummy_dealloc_setup(&range, sz);
//
//   assert_dummy_dealloc(&range, sz);
//
//   assert_dummy_alloc(&range, sz / 2);
//   ASSERT_EQ(size_t(0), free_entries());
// }
//
// TEST_P(GlobalTest, dealloc_random_alloc) {
//   ASSERT_EQ(size_t(0), free_entries());
//   const size_t sz = GetParam();
//
//   const size_t SIZE = 1024;
//   alignas(64) uint8_t range[SIZE * 64];
//   dummy_dealloc_random_setup(&range, sz);
//
//   assert_dummy_dealloc(&range, sz);
//
//   assert_dummy_alloc(&range, sz);
//   ASSERT_EQ(size_t(0), free_entries());
// }

// TODO threaded
