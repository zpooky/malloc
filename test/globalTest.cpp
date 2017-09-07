#include "Barrier.h"
#include "gtest/gtest.h"
#include <global.h>
#include <pthread.h>
#include <stdint.h>
#include <thread>

/*Gtest*/
static size_t free_entries(global::State &state) {
  auto free = test::watch_free(&state);
  return free.size();
}

struct Range {
  uint8_t *start;
  size_t length;
  Range(uint8_t *ps, size_t pl) //
      : start(ps), length(pl) {
  }
  Range() //
      : Range(nullptr, 0) {
  }
  uint8_t &operator[](size_t idx) {
    return start[idx];
  }
  const uint8_t &operator[](size_t idx) const {
    return start[idx];
  }

  uint8_t *raw_offset(size_t off) {
    assert(off < length);
    return (uint8_t *)(reinterpret_cast<uintptr_t>(start) + off);
  }
};
Range sub_range(Range &r, size_t ridx, size_t rlength) {
  size_t offset = (ridx * rlength);
  if (!(offset < r.length)) {
    assert(offset < r.length);
  }
  uint8_t *start = r.start + offset;
  return Range(start, rlength);
}

/*Parametrized Fixture*/
class GlobalTest : public testing::TestWithParam<size_t> {
public:
  global::State state;
  GlobalTest() : state() {
  }

  virtual void SetUp() {
  }
  virtual void TearDown() {
  }
};

/*Setup Parameters*/
INSTANTIATE_TEST_CASE_P(Default, GlobalTest,
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

static void dummy_dealloc_setup(global::State &state, Range &range,
                                size_t bSz) {
  size_t i = 0;
// printf("valid: ");
start:
  size_t offset = i * bSz;
  if (offset + bSz <= range.length) {
    i++;
    void *const start = (void *)range.raw_offset(offset);

    global::internal::dealloc(state, start, bSz);

    // printf("[%p,%zu]", start, bucketSz);
    goto start;
  }
  // printf("\n");
  // test::print_free();
  // printf("dummy_dealloc_setup: %zu\n", i);
}

static void dealloc_rand_setup(global::State &state, Range &range, size_t bSz) {
  std::vector<void *> points;
  size_t i = 0;
start:
  size_t offset = i * bSz;
  if (offset + bSz <= range.length) {
    i++;
    void *const start = (void *)range.raw_offset(offset);

    points.push_back(start);

    goto start;
  }

  std::random_shuffle(points.begin(), points.end());
  for (auto &start : points) {
    global::internal::dealloc(state, start, bSz);
  }
}

template <typename Frees>
static void assert_dummy_dealloc_no_abs_size(const Frees &free, Range &range) {
  // 1.
  assert_no_overlap(free);

  // 3.
  for (auto &current : free) {
    void *const cur_ptr = std::get<0>(current);
    size_t curSz = std::get<1>(current);

    if (!in_range(range.start, range.length, cur_ptr, curSz)) {
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

static void assert_dummy_dealloc_no_abs_size(global::State &s, Range &r) {
  auto free = test::watch_free(&s);
  assert_dummy_dealloc_no_abs_size(free, r);
}

static void assert_dummy_dealloc(global::State &state, Range &range) {
  auto free = test::watch_free(&state);

  ASSERT_EQ(range.length, size_of_free(free));
  ASSERT_EQ(size_t(1), free.size()); // free pages should be coalesced

  assert_dummy_dealloc_no_abs_size(free, range);
}

static void assert_dummy_alloc(global::State &state, Range &range, size_t bSz) {
  for (size_t i = 0; i < range.length; i += bSz) {

    void *const current = global::internal::alloc(state, bSz);
    ASSERT_FALSE(current == nullptr);

    if (!in_range(range.start, range.length, current, bSz)) {
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
  uint8_t *const startR = (uint8_t *)aligned_alloc(64, SIZE * 2);
  ASSERT_FALSE(startR == nullptr);
  memset(startR, 0, SIZE * 2);
  Range range(startR, SIZE);

  dummy_dealloc_setup(state, range, sz);
  for (size_t i(SIZE); i < SIZE * 2; ++i) {
    ASSERT_EQ(range[i], uint8_t(0));
  }

  assert_dummy_dealloc(state, range);
  for (size_t i(SIZE); i < SIZE * 2; ++i) {
    ASSERT_EQ(range[i], uint8_t(0));
  }

  assert_dummy_alloc(state, range, sz);
  for (size_t i(SIZE); i < SIZE * 2; ++i) {
    ASSERT_EQ(range[i], uint8_t(0));
  }

  ASSERT_EQ(size_t(0), free_entries(state));
  free(startR);
}

TEST_P(GlobalTest, dealloc_doubling_alloc) {
  const size_t sz = GetParam();

  const size_t SIZE = 1024 * 64;
  // alignas(64) uint8_t range[SIZE];
  uint8_t *const startR = (uint8_t *)aligned_alloc(64, SIZE);
  ASSERT_FALSE(startR == nullptr);
  memset(startR, 0, SIZE);
  Range range(startR, SIZE);

  //
  dummy_dealloc_setup(state, range, sz);

  //
  assert_dummy_dealloc(state, range);

  //
  assert_dummy_alloc(state, range, sz * 2);

  ASSERT_EQ(size_t(0), free_entries(state));
  free(startR);
}

TEST_P(GlobalTest, dealloc_half_alloc) {
  const size_t sz = GetParam();

  if (sz != 64) { // 64 is the minimum size
    const size_t SIZE = 1024 * 64;
    // alignas(64) uint8_t range[SIZE];
    uint8_t *const startR = (uint8_t *)aligned_alloc(64, SIZE);
    ASSERT_FALSE(startR == nullptr);
    memset(startR, 0, SIZE);
    Range range(startR, SIZE);

    dummy_dealloc_setup(state, range, sz);

    assert_dummy_dealloc(state, range);

    assert_dummy_alloc(state, range, sz / 2);

    ASSERT_EQ(size_t(0), free_entries(state));
    free(startR);
  }
}

// TEST_P(GlobalTest, dealloc_random_alloc) {
//   const size_t sz = GetParam();
//
//   dealloc_rand_setup(range, SIZE, sz);
//
//   assert_dummy_dealloc(range, SIZE);
//
//   assert_dummy_alloc(range, SIZE, sz);
//   ASSERT_EQ(size_t(0), free_entries(state));
// }

/*
 * ================================================
 * ===================THREAD=======================
 * ================================================
 */
struct ThreadAllocArg {
  int i;
  sp::Barrier *b;
  global::State *state;
  size_t sz;
  size_t thread_range_size;
  Range range;
};

static void *perform_work(void *argument) {
  auto arg = reinterpret_cast<ThreadAllocArg *>(argument);

  Range sub = sub_range(arg->range, arg->i, arg->thread_range_size);
  printf("range%d[%p,%p]\n", arg->i, sub.start, sub.start + sub.length);
  arg->b->await();
  dummy_dealloc_setup(*arg->state, sub, arg->sz);

  // assert_dummy_dealloc_no_abs_size(*arg->state, arg->range);

  // assert_dummy_alloc(*arg->state, arg->range, arg->sz);
  return nullptr;
}

TEST_P(GlobalTest, threaded_dealloc) {
  const size_t sz = GetParam();

  const size_t thCnt = 4;
  const size_t SIZE = 1024 * 64 * 8;
  const size_t RANGE_SIZE = SIZE * thCnt;

  uint8_t *const startR = (uint8_t *)aligned_alloc(64, RANGE_SIZE);
  ASSERT_FALSE(startR == nullptr);
  memset(startR, 0, RANGE_SIZE);
  Range range(startR, RANGE_SIZE);

  std::vector<pthread_t> ts;
  ThreadAllocArg args[thCnt];
  sp::Barrier b(thCnt);
  for (size_t i(0); i < thCnt; ++i) {
    auto &arg = args[i];
    {
      arg.i = i;
      arg.b = &b;
      arg.state = &state;
      arg.sz = sz;
      arg.thread_range_size = SIZE;
      arg.range = range;
    }
    pthread_t tid = 0;

    int res = pthread_create(&tid, NULL, perform_work, &arg);
    ASSERT_EQ(0, res);
    ASSERT_FALSE(tid == 0);

    ts.push_back(tid);
  }

  for (auto &t : ts) {
    int res = pthread_join(t, NULL);
    ASSERT_EQ(0, res);
  }
  assert_dummy_dealloc_no_abs_size(state, range);

  {
      // // note we assume no coalecsing
      // std::size_t frees = test::count_free(&state);
      // printf("#free objects:%zu\n", frees);
      // ASSERT_EQ(RANGE_SIZE / sz, frees);
  } //
  {
    // TODO
    // test::coalesce_free(&state);
    // ASSERT_EQ(size_t(1), test::count_free(&state));
    // assert_dummy_dealloc(state, range);
  }

  free(startR);
}
// > make -C test && sp_repeat ./test/thetest.exe --gtest_filter="*threaded_dealloc*"

// TODO threaded
// TODO shuffle inc size dealloc
// TODO increment size dealloc
// TODO decrementing size dealloc
