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

static void assert_in_range(Range &range, void *current, size_t bSz) {
  if (!in_range(range.start, range.length, current, bSz)) {
    printf("in_range(%p, %zu, %p, "
           "%zu)|range(%zu-%zu[l:%zu])|cur(%zu-%zu[l:%zu])\n",      //
           range.start, range.length, current, bSz,                 //
           reinterpret_cast<uintptr_t>(range.start),                //
           reinterpret_cast<uintptr_t>(range.start) + range.length, //
           range.length,                                            //
           reinterpret_cast<uintptr_t>(current),                    //
           reinterpret_cast<uintptr_t>(current) + bSz,              //
           bSz);

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
    // printf("%p > %p and %p < %p and %p <= %p\n", //
    //        rangeStart, current, current, rangeEnd, curEnd, rangeEnd);
    ASSERT_TRUE(false);
  }
}

template <typename Points>
static void assert_no_overlap(const Points &ptrs) {
  // printf("assert_no_overlap(points:%zu)\n", ptrs.size());
  auto current = ptrs.begin();
  while (current != ptrs.end()) {
    auto it = current;
    while (++it != ptrs.end()) {
      if (in_range(*current, *it)) {
        // printf("    current[%p, %zu]\n\t it[%p, %zu]",       //
        //        std::get<0>(*current), std::get<1>(*current), //
        //        std::get<0>(*it), std::get<1>(*it));
        printf(" == false\n");
        ASSERT_TRUE(false);
      } else {
        // printf(" == true\n");
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

    // printf("global::internal::dealloc(state, %p, %zu)\n", start, bSz);
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

template <typename Ts>
static void assert_dummy_dealloc_no_abs_size(const std::vector<Ts> &free,
                                             Range &range) {
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

  //
  assert_no_overlap(free);
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

static auto assert_find_free(global::State &state, size_t size, Range &range,
                             size_t bSz) {
  std::vector<std::tuple<void *, std::size_t>> result;
  for (size_t i = 0; i < size; i += bSz) {

  retry:
    void *const current = global::internal::find_freex(state, bSz);
    if (current == nullptr) {
      goto retry;
    }
    assert_in_range(range, current, bSz);
    result.emplace_back(current, bSz);
  }

  return result;
}

static void
assert_dummy_alloc(global::State &state, size_t size, Range &range, size_t bSz,
                   std::vector<std::tuple<void *, std::size_t>> &result) {

  for (size_t i = 0; i < size; i += bSz) {
  retry:
    void *const current = global::internal::find_freex(state, bSz);
    if (current == nullptr) {
      // printf("null\m");
      goto retry;
    }
    ASSERT_FALSE(current == nullptr);

    assert_in_range(range, current, bSz);
    result.emplace_back(current, bSz);
  }
}

static void
assert_consecutive_range(std::vector<std::tuple<void *, std::size_t>> &free,
                         Range range) {
  uint8_t *const startR = range.start;

  auto cmp = [](const auto &first, const auto &second) -> bool {
    return std::get<0>(first) < std::get<0>(second);
  };
  std::sort(free.begin(), free.end(), cmp);

  void *first = startR;
  for (auto it = free.begin(); it != free.end(); ++it) {
    ASSERT_EQ(std::get<0>(*it), first);

    uintptr_t fptr = reinterpret_cast<uintptr_t>(std::get<0>(*it));
    first = reinterpret_cast<void *>(fptr + std::get<1>(*it));
  }
  ASSERT_EQ(first, range.start + range.length);
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

  std::vector<std::tuple<void *, std::size_t>> result;
  assert_dummy_alloc(state, range.length, range, sz, result);
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
  std::vector<std::tuple<void *, std::size_t>> result;
  assert_dummy_alloc(state, range.length, range, sz * 2, result);

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

    std::vector<std::tuple<void *, std::size_t>> result;
    assert_dummy_alloc(state, range.length, range, sz / 2, result);

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
  sp::Barrier *b1;
  sp::Barrier *b2;
  sp::Barrier *b3;
  sp::Barrier *b4;

  global::State *state;
  size_t sz;
  size_t thread_range_size;
  Range range;
  std::vector<std::tuple<void *, std::size_t>> result;
};

template <size_t thCnt>
static void threaded(global::State &state, size_t sz, size_t SIZE,
                     const Range &range,
                     const std::vector<void *(*)(void *)> &fs,
                     std::vector<std::tuple<void *, std::size_t>> &result) {

  std::vector<pthread_t> ts;
  ThreadAllocArg args[thCnt];
  sp::Barrier b(thCnt);
  sp::Barrier b1(thCnt);
  sp::Barrier b2(thCnt);
  sp::Barrier b3(thCnt);
  sp::Barrier b4(thCnt);

  for (size_t i(0); i < thCnt; ++i) {
    auto &arg = args[i];
    {
      arg.i = i;
      arg.b = &b;
      arg.b1 = &b1;
      arg.b2 = &b2;
      arg.b3 = &b3;
      arg.b4 = &b4;
      arg.state = &state;
      arg.sz = sz;
      arg.thread_range_size = SIZE;
      arg.range = range;
    }
    pthread_t tid = 0;

    int res = pthread_create(&tid, NULL, fs[i % fs.size()], &arg);
    ASSERT_EQ(0, res);
    ASSERT_FALSE(tid == 0);

    ts.push_back(tid);
  }

  for (size_t i(0); i < thCnt; ++i) {
    int res = pthread_join(ts[i], NULL);
    ASSERT_EQ(0, res);

    std::atomic_thread_fence(std::memory_order_acquire);

    std::vector<std::tuple<void *, std::size_t>> &curResult = args[i].result;
    result.insert(result.end(), curResult.begin(), curResult.end());
  }
}
//==========================================
//=======threaded=dealloc===================
//==========================================

static void *worker_random_dealloc(void *argument) {
  auto arg = reinterpret_cast<ThreadAllocArg *>(argument);

  Range sub = sub_range(arg->range, arg->i, arg->thread_range_size);
  arg->b->await();
  dummy_dealloc_setup(*arg->state, sub, arg->sz);

  std::atomic_thread_fence(std::memory_order_release);
  return nullptr;
}

static void *worker_dealloc(void *argument) {
  auto arg = reinterpret_cast<ThreadAllocArg *>(argument);

  Range sub = sub_range(arg->range, arg->i, arg->thread_range_size);
  // printf("range%d[%p,%p]\n", arg->i, sub.start, sub.start + sub.length);
  arg->b->await();
  // printf("dealloc start\n");
  // printf("arg->sz: %zu\n", arg->sz);
  dummy_dealloc_setup(*arg->state, sub, arg->sz);

  // printf("dealloc done\n");
  std::atomic_thread_fence(std::memory_order_release);
  return nullptr;
}

static void threaded_dealloc_test(global::State &state, size_t sz,
                                  void *(*f)(void *)) {
  const size_t thCnt = 4;
  const size_t SIZE = 1024 * 64 * 8;
  const size_t RANGE_SIZE = SIZE * thCnt;
  // printf("range_size: %zu\n", RANGE_SIZE);

  uint8_t *const startR = (uint8_t *)aligned_alloc(64, RANGE_SIZE);
  ASSERT_FALSE(startR == nullptr);
  memset(startR, 0, RANGE_SIZE);
  Range range(startR, RANGE_SIZE);

  std::vector<std::tuple<void *, std::size_t>> result;
  threaded<thCnt>(state, sz, SIZE, range, {f}, result);
  // test::print_free(&state);
  assert_dummy_dealloc_no_abs_size(state, range);

  auto frees = test::watch_free(&state);
  assert_consecutive_range(frees, range);

  free(startR);
}

TEST_P(GlobalTest, threaded_random_dealloc) {
  const size_t sz = GetParam();
  threaded_dealloc_test(state, sz, worker_random_dealloc);
}

TEST_P(GlobalTest, threaded_dealloc) {
  const size_t sz = GetParam();
  threaded_dealloc_test(state, sz, worker_dealloc);
}
//==========================================
//=======threaded=dealloc=alloc=============
//==========================================sp
static void *worker_dealloc_alloc(void *argument) {
  auto arg = reinterpret_cast<ThreadAllocArg *>(argument);

  Range sub = sub_range(arg->range, arg->i, arg->thread_range_size);
  // printf("range%d[%p,%p]\n", arg->i, sub.start, sub.start + sub.length);
  arg->b->await();

  global::State &state = *arg->state;
  size_t thread_range_size = arg->thread_range_size;

  printf("arg->b->await();\n");
  // printf("arg->sz: %zu\n", arg->sz);
  dummy_dealloc_setup(state, sub, arg->sz);

  {
    arg->b1->await();
    printf("arg->b1->await();\n");

    assert_dummy_dealloc_no_abs_size(state, arg->range);
    auto frees = test::watch_free(&state);
    assert_no_overlap(frees);
    assert_consecutive_range(frees, arg->range);

    printf("arg->b2->await();\n");
    arg->b2->await();
  }

  std::vector<std::tuple<void *, std::size_t>> result;
  assert_dummy_alloc(state, thread_range_size, arg->range, arg->sz, result);
  assert_no_overlap(result);

  std::atomic_thread_fence(std::memory_order_release);
  return nullptr;
}

static void *worker_random_dealloc_alloc(void *argument) {
  auto arg = reinterpret_cast<ThreadAllocArg *>(argument);

  Range sub = sub_range(arg->range, arg->i, arg->thread_range_size);
  // printf("range%d[%p,%p]\n", arg->i, sub.start, sub.start + sub.length);
  arg->b->await();
  // printf("arg->sz: %zu\n", arg->sz);
  dealloc_rand_setup(*arg->state, sub, arg->sz);

  std::vector<std::tuple<void *, std::size_t>> result;
  assert_dummy_alloc(*arg->state, arg->thread_range_size, arg->range, arg->sz,
                     result);
  assert_no_overlap(result);

  std::atomic_thread_fence(std::memory_order_release);
  return nullptr;
}

static void threaded_dealloc_alloc_test(global::State &state, size_t sz,
                                        void *(*f)(void *)) {
  const size_t thCnt = 2;
  const size_t SIZE = 1024 * 64 * 8;
  const size_t RANGE_SIZE = SIZE * thCnt;
  // printf("range_size: %zu\n", RANGE_SIZE);

  uint8_t *const startR = (uint8_t *)aligned_alloc(64, RANGE_SIZE);
  ASSERT_FALSE(startR == nullptr);
  memset(startR, 0, RANGE_SIZE);
  Range range(startR, RANGE_SIZE);

  std::vector<std::tuple<void *, std::size_t>> result;
  threaded<thCnt>(state, sz, SIZE, range, {f}, result);

  assert_consecutive_range(result, range);

  ASSERT_EQ(size_t(0), test::count_free(&state));

  free(startR);
}

TEST_P(GlobalTest, threaded_dealloc_alloc) {
  const size_t sz = GetParam();
  threaded_dealloc_alloc_test(state, sz, worker_dealloc_alloc);
}

TEST_P(GlobalTest, threaded_random_dealloc_alloc) {
  const size_t sz = GetParam();
  threaded_dealloc_alloc_test(state, sz, worker_random_dealloc_alloc);
}
//================================
static void *worker_alloc(void *argument) {
  auto arg = reinterpret_cast<ThreadAllocArg *>(argument);

  arg->b->await();
  // printf("alloc start\n");

  auto result = assert_find_free(*arg->state, arg->thread_range_size,
                                 arg->range, arg->sz);
  assert_no_overlap(result);

  // printf("alloc done\n");
  std::atomic_thread_fence(std::memory_order_release);
  return 0;
}

static void threaded_dealloc_threaded_alloc_test(global::State &state,
                                                 size_t sz, //
                                                 void *(*wa)(void *),
                                                 void *(*wd)(void *)) {
  const size_t thCnt = 4;
  const size_t SIZE = 1024 * 64 * 8;
  const size_t RANGE_SIZE = SIZE * thCnt;
  // printf("range_size: %zu\n", RANGE_SIZE);

  uint8_t *const startR = (uint8_t *)aligned_alloc(64, RANGE_SIZE);
  ASSERT_FALSE(startR == nullptr);
  memset(startR, 0, RANGE_SIZE);
  Range range(startR, RANGE_SIZE);

  std::vector<std::tuple<void *, std::size_t>> result;
  threaded<thCnt>(state, sz, SIZE, range, {wd, wa}, result);
  assert_consecutive_range(result, range);

  ASSERT_EQ(size_t(0), test::count_free(&state));

  free(startR);
}

TEST_P(GlobalTest, threaded_dealloc_threaded_alloc_test) {
  const size_t sz = GetParam();
  threaded_dealloc_threaded_alloc_test(state, sz, worker_alloc, worker_dealloc);
}

TEST_P(GlobalTest, threaded_random_dealloc_threaded_alloc_test) {
  const size_t sz = GetParam();
  threaded_dealloc_threaded_alloc_test(state, sz, worker_alloc,
                                       worker_random_dealloc);
}
// > make -C test && sp_repeat ./test/thetest.exe
// --gtest_filter="*threaded_dealloc*"

// TODO shuffle inc size dealloc
// TODO increment size dealloc
// TODO decrementing size dealloc
