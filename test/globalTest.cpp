#include "Barrier.h"
#include "Util.h"
#include "gtest/gtest.h"
#include <global.h>
#include <global_debug.h>
#include <pthread.h>
#include <stdint.h>
#include <thread>

/*Gtest*/
static size_t
free_entries(global::State &state) {
  auto free = debug::global_get_free(&state);
  return free.size();
}

/*Parametrized Fixture*/
class GlobalTest : public testing::TestWithParam<size_t> {
public:
  global::State state;
  GlobalTest()
      : state() {
  }

  virtual void
  SetUp() {
  }
  virtual void
  TearDown() {
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

static void
dummy_dealloc_setup(global::State &state, Range &range, sp::node_size bSz) {
  // printf("dummy_dealloc_setup\n");
  std::size_t i = 0;
// printf("valid: ");
start:
  sp::node_size offset(bSz * i);
  if (offset + bSz <= range.length) {
    i++;
    void *const start = (void *)range.raw_offset(offset);

    // printf("global::internal::dealloc(state, %p, %zu)\n", start, bSz);
    global::internal::dealloc(state, start, bSz);

    // printf("[%p,%zu]", start, bucketSz);
    goto start;
  }
  ASSERT_EQ(offset, range.length);
  // printf("\n");
  // debug::print_free();
  // printf("dummy_dealloc_setup: %zu\n", i);
}

static void
dealloc_rand_setup(global::State &state, Range &range, sp::node_size bSz) {
  std::vector<void *> points;
  std::size_t i = 0;
start:
  sp::node_size offset(bSz * i);
  if (offset + bSz <= range.length) {
    i++;
    void *const start = (void *)range.raw_offset(offset);

    points.push_back(start);

    goto start;
  }
  ASSERT_EQ(offset, range.length);

  std::random_shuffle(points.begin(), points.end());
  for (auto &start : points) {
    global::internal::dealloc(state, start, bSz);
  }
}

template <typename Ts>
static void
assert_dummy_dealloc_no_abs_size(const std::vector<Ts> &free, Range &range) {
  // printf("assert_dummy_dealloc_no_abs_size\n");
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

static void
assert_dummy_dealloc_no_abs_size(global::State &s, Range &r) {
  auto free = debug::global_get_free(&s);
  assert_dummy_dealloc_no_abs_size(free, r);
}

static void
assert_dummy_dealloc(global::State &state, Range &range) {
  auto free = debug::global_get_free(&state);

  ASSERT_EQ(range.length, size_of_free(free));
  ASSERT_EQ(size_t(1), free.size()); // free pages should be coalesced

  assert_dummy_dealloc_no_abs_size(free, range);
}

static void
assert_dummy_alloc(global::State &state, size_t size, Range &range,
                   sp::node_size bSz, Points &result) {

  // printf("assert_dummy_dealloc\n");
  for (size_t i = 0; i < size; i += std::size_t(bSz)) {
  retry:
    void *const current = global::internal::find_freex(state, bSz);
    if (current == nullptr) {
      // printf("null\m");
      goto retry;
    }

    assert_in_range(range, current, bSz);
    result.emplace_back(current, bSz);
  }
}

/*Test*/
TEST_P(GlobalTest, dealloc_alloc_symmetric) {
  const sp::node_size sz(GetParam());

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

  Points result;
  assert_dummy_alloc(state, range.length, range, sz, result);
  for (size_t i(SIZE); i < SIZE * 2; ++i) {
    ASSERT_EQ(range[i], uint8_t(0));
  }

  ASSERT_EQ(size_t(0), free_entries(state));
  free(startR);
}

TEST_P(GlobalTest, dealloc_doubling_alloc) {
  const sp::node_size sz(GetParam());

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
  Points result;
  assert_dummy_alloc(state, range.length, range, sz * 2, result);

  ASSERT_EQ(size_t(0), free_entries(state));
  free(startR);
}

TEST_P(GlobalTest, dealloc_half_alloc) {
  const sp::node_size sz(GetParam());

  if (sz != 64) { // 64 is the minimum size
    const size_t SIZE = 1024 * 64;
    // alignas(64) uint8_t range[SIZE];
    uint8_t *const startR = (uint8_t *)aligned_alloc(64, SIZE);
    ASSERT_FALSE(startR == nullptr);
    memset(startR, 0, SIZE);
    Range range(startR, SIZE);

    dummy_dealloc_setup(state, range, sz);

    assert_dummy_dealloc(state, range);

    Points result;
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
  sp::node_size sz;
  size_t thread_range_size;
  Range range;
  Points result;
  ThreadAllocArg()
      : i(0)
      , b(nullptr)
      , b1(nullptr)
      , b2(nullptr)
      , b3(nullptr)
      , b4(nullptr)
      , state(nullptr)
      , sz(0)
      , thread_range_size(0)
      , range()
      , result() {
  }
};

template <size_t thCnt>
static void
join(std::vector<pthread_t> &ts, ThreadAllocArg (&args)[thCnt],
     Points &result) {
  for (size_t i(0); i < thCnt; ++i) {
    int res = pthread_join(ts[i], NULL);
    ASSERT_EQ(0, res);

    std::atomic_thread_fence(std::memory_order_acquire);

    Points &curResult = args[i].result;
    result.insert(result.end(), curResult.begin(), curResult.end());
  }
}

template <size_t thCnt>
static void
threaded(global::State &state, size_t sz, size_t SIZE, const Range &range,
         const std::vector<void *(*)(void *)> &fs, Points &result) {

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
  join(ts, args, result);
}
//==========================================
//=======threaded=dealloc===================
//==========================================

static void *
worker_random_dealloc(void *argument) {
  auto arg = reinterpret_cast<ThreadAllocArg *>(argument);

  Range sub = sub_range(arg->range, arg->i, arg->thread_range_size);
  arg->b->await();
  dummy_dealloc_setup(*arg->state, sub, arg->sz);

  std::atomic_thread_fence(std::memory_order_release);
  return nullptr;
}

static void *
worker_dealloc(void *argument) {
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

static void
threaded_dealloc_test(global::State &state, size_t sz, void *(*f)(void *)) {
  const size_t thCnt = 4;
  const size_t SIZE = 1024 * 64 * 8;
  const size_t RANGE_SIZE = SIZE * thCnt;
  // printf("range_size: %zu\n", RANGE_SIZE);

  uint8_t *const startR = (uint8_t *)aligned_alloc(64, RANGE_SIZE);
  ASSERT_FALSE(startR == nullptr);
  memset(startR, 0, RANGE_SIZE);
  Range range(startR, RANGE_SIZE);

  Points result;
  threaded<thCnt>(state, sz, SIZE, range, {f}, result);
  // debug::print_free(&state);
  assert_dummy_dealloc_no_abs_size(state, range);

  auto frees = debug::global_get_free(&state);
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
//==========================================
static void *
worker_dealloc_alloc(void *argument) {
  auto arg = reinterpret_cast<ThreadAllocArg *>(argument);

  Range subRange = sub_range(arg->range, arg->i, arg->thread_range_size);
  // printf("range%d[%p,%p]\n", arg->i, sub.start, sub.start + sub.length);
  arg->b->await();
  // printf("arg->b->await();\n");

  global::State &state = *arg->state;
  size_t thread_range_size = arg->thread_range_size;

  // printf("arg->sz: %zu\n", arg->sz);
  dummy_dealloc_setup(state, subRange, arg->sz);

  // {
  //   arg->b1->await();
  //   printf("arg->b1->await();\n");
  //
  //   assert_dummy_dealloc_no_abs_size(state, arg->range);
  //   auto frees = debug::watch_free(&state);
  //   assert_no_overlap(frees);
  //   assert_consecutive_range(frees, arg->range);
  //
  //   printf("arg->b2->await();\n");
  //   arg->b2->await();
  //   printf("alloc\n");
  // }

  assert_dummy_alloc(state, thread_range_size, arg->range, arg->sz,
                     arg->result);
  assert_no_overlap(arg->result);

  std::atomic_thread_fence(std::memory_order_release);
  return nullptr;
}

static void *
worker_random_dealloc_alloc(void *argument) {
  auto arg = reinterpret_cast<ThreadAllocArg *>(argument);

  Range sub = sub_range(arg->range, arg->i, arg->thread_range_size);
  // printf("range%d[%p,%p]\n", arg->i, sub.start, sub.start + sub.length);
  arg->b->await();
  // printf("arg->sz: %zu\n", arg->sz);

  global::State &state = *arg->state;
  size_t thread_range_size = arg->thread_range_size;

  dealloc_rand_setup(state, sub, arg->sz);

  assert_dummy_alloc(state, thread_range_size, arg->range, arg->sz,
                     arg->result);
  assert_no_overlap(arg->result);

  std::atomic_thread_fence(std::memory_order_release);
  return nullptr;
}

static void
threaded_dealloc_alloc_test(global::State &state, size_t sz,
                            void *(*f)(void *)) {
  const size_t thCnt = 4;
  const size_t SIZE = 1024 * 64 * 8;
  const size_t RANGE_SIZE = SIZE * thCnt;
  // printf("range_size: %zu\n", RANGE_SIZE);

  uint8_t *const startR = (uint8_t *)aligned_alloc(64, RANGE_SIZE);
  ASSERT_FALSE(startR == nullptr);
  memset(startR, 0, RANGE_SIZE);
  Range range(startR, RANGE_SIZE);

  Points result;
  threaded<thCnt>(state, sz, SIZE, range, {f}, result);

  assert_consecutive_range(result, range);

  ASSERT_EQ(size_t(0), debug::count_free(&state));

  free(startR);
}

TEST_P(GlobalTest, threaded_dealloc_alloc) {
  const size_t sz = GetParam();
  threaded_dealloc_alloc_test(state, sz, worker_dealloc_alloc);
}

TEST_P(GlobalTest, threaded_random_dealloc_alloc) { // spx
  const size_t sz = GetParam();
  threaded_dealloc_alloc_test(state, sz, worker_random_dealloc_alloc);
}
//================================
static void *
worker_alloc(void *argument) {
  auto arg = reinterpret_cast<ThreadAllocArg *>(argument);

  arg->b->await();
  // printf("alloc start\n");

  std::size_t rangeSz = arg->thread_range_size;
  assert_dummy_alloc(*arg->state, rangeSz, arg->range, arg->sz, arg->result);
  assert_no_overlap(arg->result);

  // printf("alloc done\n");
  std::atomic_thread_fence(std::memory_order_release);
  return 0;
}

static void
threaded_dealloc_threaded_alloc_test(global::State &state,
                                     size_t sz, //
                                     void *(*wa)(void *), void *(*wd)(void *)) {
  const size_t thPerType = 4;
  const size_t thAlloc = thPerType;
  const size_t thDealloc = thPerType;
  const size_t thCnt = thAlloc + thDealloc;

  const size_t SIZE = 1024 * 64 * 8;
  const size_t RANGE_SIZE = SIZE * thDealloc;
  // printf("range_size: %zu\n", RANGE_SIZE);

  uint8_t *const startR = (uint8_t *)aligned_alloc(64, RANGE_SIZE);
  ASSERT_FALSE(startR == nullptr);
  memset(startR, 0, RANGE_SIZE);
  Range range(startR, RANGE_SIZE);

  std::vector<pthread_t> ts;
  ThreadAllocArg args[thCnt];
  sp::Barrier b(thCnt);
  sp::Barrier b1(thCnt);
  sp::Barrier b2(thCnt);
  sp::Barrier b3(thCnt);
  sp::Barrier b4(thCnt);

  size_t des = 0;
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

    auto current_worker = wa;
    if (i % 2 == 0) {
      arg.i = des++;
      current_worker = wd;
    }

    int res = pthread_create(&tid, nullptr, current_worker, &arg);
    ASSERT_EQ(0, res);
    ASSERT_FALSE(tid == 0);

    ts.push_back(tid);
  }

  Points result;
  join(ts, args, result);

  assert_consecutive_range(result, range);

  ASSERT_EQ(size_t(0), debug::count_free(&state));

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
// make -C test && sp_repeat ./test/thetest.exe
// --gtest_filter="*threaded_dealloc*"

// TODO shuffle inc size dealloc
// TODO increment size dealloc
// TODO decrementing size dealloc
// TODO cartestian combination of alloc&dealloc threads
