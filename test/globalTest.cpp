#include "Barrier.h"
#include "gtest/gtest.h"
#include <global.h>
#include <stdint.h>
#include <thread>

/*Gtest*/
static size_t free_entries(global::State &state) {
  auto free = test::watch_free(&state);
  return free.size();
}

/*Parametrized Fixture*/
class GlobalTest : public testing::TestWithParam<size_t> {
public:
  global::State state;
  GlobalTest() : state() {
  }

  virtual void SetUp() {
    ASSERT_EQ(size_t(0), free_entries(state));
  }
  virtual void TearDown() {
    ASSERT_EQ(size_t(0), free_entries(state));
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

/*Util*/
static bool in_range(void *r, size_t rSz, void *e, size_t eLen) {
  uintptr_t in = reinterpret_cast<uintptr_t>(e);
  uintptr_t inEnd = in + eLen;

  uintptr_t rangeStart = reinterpret_cast<uintptr_t>(r);
  uintptr_t rangeEnd = rangeStart + rSz;

  return (in >= rangeStart && in < rangeEnd) && (inEnd <= rangeEnd);
}

template <typename T>
static size_t size_of_free(const T &free) {
  size_t result(0);
  for (const auto &current : free) {
    result += std::get<1>(current);
  }
  return result;
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
    current++;
  }
}

static void dummy_dealloc_setup(global::State &state, uint8_t *const range,
                                size_t rSz, size_t bSz) {
  size_t i = 0;
// printf("valid: ");
start:
  size_t offset = i * bSz;
  if (offset + bSz <= rSz) {
    i++;
    void *const start = (void *)(reinterpret_cast<uintptr_t>(range) + offset);

    global::internal::dealloc(state, start, bSz);

    // printf("[%p,%zu]", start, bucketSz);
    goto start;
  }
  // printf("\n");
  // test::print_free();
  // printf("dummy_dealloc_setup: %zu\n", i);
}

static void dealloc_rand_setup(global::State &state, uint8_t *const range,
                               size_t rSz, size_t bSz) {
  std::vector<void *> points;
  size_t i = 0;
start:
  size_t offset = i * bSz;
  if (offset + bSz <= rSz) {
    i++;
    void *const start = (void *)(reinterpret_cast<uintptr_t>(range) + offset);

    points.push_back(start);

    goto start;
  }

  std::random_shuffle(points.begin(), points.end());
  for (auto &start : points) {
    global::internal::dealloc(state, start, bSz);
  }
}

static void assert_dummy_dealloc(global::State &state, uint8_t *const range,
                                 size_t rSz) {
  auto free = test::watch_free(&state);

  // 1.
  assert_no_overlap(free);

  // 2.
  ASSERT_EQ(rSz, size_of_free(free));
  ASSERT_EQ(size_t(1), free.size()); // free pages should be coalesced

  // 3.
  for (auto &current : free) {
    void *const cur_ptr = std::get<0>(current);
    size_t curSz = std::get<1>(current);

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

static void assert_dummy_alloc(global::State &state, uint8_t *const range,
                               size_t rSz, size_t bSz) {
  for (size_t i = 0; i < rSz; i += bSz) {

    void *const current = global::internal::alloc(state, bSz);
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

/*Test*/
TEST_P(GlobalTest, dealloc_alloc_symmetric) {
  const size_t sz = GetParam();

  const size_t SIZE = 1024 * 64;
  // alignas(64) uint8_t range[SIZE];
  uint8_t *const range = (uint8_t *)aligned_alloc(64, SIZE * 2);
  memset(range, 0, SIZE * 2);

  dummy_dealloc_setup(state, range, SIZE, sz);
  for (size_t i(SIZE); i < SIZE * 2; ++i) {
    ASSERT_EQ(range[i], uint8_t(0));
  }

  assert_dummy_dealloc(state, range, SIZE);
  for (size_t i(SIZE); i < SIZE * 2; ++i) {
    ASSERT_EQ(range[i], uint8_t(0));
  }

  assert_dummy_alloc(state, range, SIZE, sz);
  for (size_t i(SIZE); i < SIZE * 2; ++i) {
    ASSERT_EQ(range[i], uint8_t(0));
  }

  free(range);
}

TEST_P(GlobalTest, dealloc_doubling_alloc) {
  const size_t sz = GetParam();

  const size_t SIZE = 1024 * 64;
  alignas(64) uint8_t range[SIZE];

  //
  dummy_dealloc_setup(state, range, SIZE, sz);

  //
  assert_dummy_dealloc(state, range, SIZE);

  //
  assert_dummy_alloc(state, range, SIZE, sz * 2);
}

TEST_P(GlobalTest, dealloc_half_alloc) {
  const size_t sz = GetParam();

  if (sz != 64) { // 64 is the minimum size
    const size_t SIZE = 1024 * 64;
    alignas(64) uint8_t range[SIZE];

    dummy_dealloc_setup(state, range, SIZE, sz);

    assert_dummy_dealloc(state, range, SIZE);

    assert_dummy_alloc(state, range, SIZE, sz / 2);
  }
}

// TEST_P(GlobalTest, dealloc_random_alloc) {
//   const size_t sz = GetParam();
//
//   const size_t SIZE = 1024 * 64;
//   alignas(64) uint8_t range[SIZE];
//
//   dealloc_rand_setup(range, SIZE, sz);
//
//   assert_dummy_dealloc(range, SIZE);
//
//   assert_dummy_alloc(range, SIZE, sz);
// }

/*
 * ================================================
 * ===================THREAD=======================
 * ================================================
 */
TEST_P(GlobalTest, threaded_dealloc_alloc_symmetric) {
  const size_t sz = GetParam();

  const size_t thCnt = 2;
  const size_t SIZE = 1024 * 64;
  const size_t RANGE_SIZE = SIZE * thCnt;

  uint8_t *const range = (uint8_t *)aligned_alloc(64, RANGE_SIZE);
  memset(range, 0, RANGE_SIZE);

  std::vector<std::thread> ts;
  sp::Barrier b(thCnt);
  for (size_t i(0); i < thCnt; ++i) {
    global::State &statex = state;
    ts.emplace_back([i, &b, &statex, sz, &range] {
      b.await();

      dummy_dealloc_setup(statex, range + (i * SIZE), SIZE, sz);

      assert_dummy_dealloc(statex, range, RANGE_SIZE);

      assert_dummy_alloc(statex, range, RANGE_SIZE, sz);

      free(range);
    });
  }

  for (auto &t : ts) {
    t.join();
  }
}

// TODO threaded
// TODO shuffle inc size dealloc
// TODO increment size dealloc
// TODO decrementing size dealloc
