#include "Barrier.h"
#include "ReadWriteLock.h"
#include "gtest/gtest.h"
#include <list>
#include <pthread.h>

using namespace sp;

void assert_shared(ReadWriteLock &lock) {
  ASSERT_TRUE(lock.shared_locks() > 0);
}

void assert_not_shared(ReadWriteLock &lock) {
  ASSERT_FALSE(lock.shared_locks() > 0);
}

void assert_prepare(ReadWriteLock &lock) {
  ASSERT_TRUE(lock.has_prepare_lock());
}

void assert_not_prepare(ReadWriteLock &lock) {
  ASSERT_FALSE(lock.has_prepare_lock());
}

void assert_exclusive(ReadWriteLock &lock) {
  ASSERT_TRUE(lock.has_exclusive_lock());
}

void assert_not_exclusive(ReadWriteLock &lock) {
  ASSERT_FALSE(lock.has_exclusive_lock());
}

void assert_only_shared(ReadWriteLock &lock) {
  assert_shared(lock);
  assert_not_prepare(lock);
  assert_not_exclusive(lock);
}

void assert_only_prepare(ReadWriteLock &lock) {
  assert_not_shared(lock);
  assert_prepare(lock);
  assert_not_exclusive(lock);
}

void assert_only_exclusive(ReadWriteLock &lock) {
  assert_not_shared(lock);
  assert_not_prepare(lock);
  assert_exclusive(lock);
}

void assert_only_shared_prepare(ReadWriteLock &lock) {
  assert_shared(lock);
  assert_prepare(lock);
  assert_not_exclusive(lock);
}

void assert_acq(ReadWriteLock &lock) {
  assert_not_shared(lock);
  assert_not_prepare(lock);
  assert_not_exclusive(lock);
}

TEST(ReadWriteLockTest, test_shared) {
  ReadWriteLock lock;
  assert_acq(lock);
  {
    SharedLock sl(lock);
    ASSERT_TRUE(sl);
    assert_only_shared(lock);
  }
  assert_acq(lock);
  {
    SharedLock sl(lock);
    ASSERT_TRUE(sl);
    assert_only_shared(lock);
  }
  assert_acq(lock);
  {
    assert_acq(lock);
    TrySharedLock sl(lock);
    ASSERT_TRUE(sl);
    assert_only_shared(lock);
  }
  assert_acq(lock);
  {
    assert_acq(lock);
    int i(0);
    std::list<SharedLock> locks;
    while (i++ < 100) {
      locks.emplace_back(lock);
      assert_only_shared(lock);
    }
    for (const auto &l : locks) {
      ASSERT_TRUE(l);
    }
  }
  assert_acq(lock);
  {
    int i(0);
    std::list<TrySharedLock> locks;
    while (i++ < 100) {
      locks.emplace_back(lock);
      assert_only_shared(lock);
    }
    for (const auto &l : locks) {
      ASSERT_TRUE(l);
    }
  }
  assert_acq(lock);
  {
    int i(0);
    std::list<TrySharedLock> tslocks;
    std::list<SharedLock> slocks;
    while (i++ < 100) {
      tslocks.emplace_back(lock);
      assert_only_shared(lock);
      slocks.emplace_back(lock);
      assert_only_shared(lock);
    }
    for (const auto &l : slocks) {
      ASSERT_TRUE(l);
    }
    for (const auto &l : tslocks) {
      ASSERT_TRUE(l);
    }
  }
  assert_acq(lock);
}

TEST(ReadWriteLockTest, test_prepare) {
  ReadWriteLock lock;
  assert_acq(lock);
  { //
    TryPrepareLock pl(lock);
    ASSERT_TRUE(pl);
    assert_only_prepare(lock);
  }
  assert_acq(lock);
  { //
    TryPrepareLock pl(lock);
    ASSERT_TRUE(pl);
    assert_only_prepare(lock);
    TryPrepareLock pl2(lock);
    ASSERT_FALSE(pl2);
    ASSERT_TRUE(pl);
    assert_only_prepare(lock);
  }
  assert_acq(lock);
}

TEST(ReadWriteLockTest, test_exclusive) {
  ReadWriteLock lock;
  assert_acq(lock);
  { //
    TryExclusiveLock el(lock);
    ASSERT_TRUE(el);
    assert_only_exclusive(lock);
  }
  assert_acq(lock);
  { //
    EagerExclusiveLock el(lock);
    ASSERT_TRUE(el);
    assert_only_exclusive(lock);
  }
  { //
    LazyExclusiveLock el(lock);
    ASSERT_TRUE(el);
    assert_only_exclusive(lock);
  }
  assert_acq(lock);
  { //
    TryExclusiveLock el(lock);
    ASSERT_TRUE(el);
    assert_only_exclusive(lock);
    TryExclusiveLock el2(lock);
    ASSERT_FALSE(el2);
    ASSERT_TRUE(el);
    assert_only_exclusive(lock);
  }
  assert_acq(lock);
  { //
    EagerExclusiveLock el(lock);
    ASSERT_TRUE(el);
    assert_only_exclusive(lock);
    TryExclusiveLock el2(lock);
    ASSERT_FALSE(el2);
    ASSERT_TRUE(el);
    assert_only_exclusive(lock);
  }
  assert_acq(lock);
  { //
    LazyExclusiveLock el(lock);
    ASSERT_TRUE(el);
    assert_only_exclusive(lock);
    TryExclusiveLock el2(lock);
    ASSERT_FALSE(el2);
    ASSERT_TRUE(el);
    assert_only_exclusive(lock);
  }
  assert_acq(lock);
}

TEST(ReadWriteLockTest, test_shared_exclusive) {
  ReadWriteLock lock;
  assert_acq(lock);
  { //
    SharedLock sl(lock);
    ASSERT_TRUE(sl);
    assert_only_shared(lock);
    TryExclusiveLock el(lock);
    ASSERT_FALSE(el);
    assert_only_shared(lock);

    TrySharedLock sl2(lock);
    ASSERT_TRUE(sl2);
    assert_only_shared(lock);
    SharedLock sl3(lock);
    ASSERT_TRUE(sl3);
    assert_only_shared(lock);
    TryExclusiveLock el2(lock);
    ASSERT_FALSE(el2);
    assert_only_shared(lock);
  }
  assert_acq(lock);
  { //
    TryExclusiveLock el(lock);
    ASSERT_TRUE(el);
    TrySharedLock sl(lock);
    ASSERT_FALSE(sl);
    assert_only_exclusive(lock);
  }
  assert_acq(lock);
  { //
    EagerExclusiveLock el(lock);
    ASSERT_TRUE(el);
    TrySharedLock sl(lock);
    ASSERT_FALSE(sl);
    assert_only_exclusive(lock);
  }
  assert_acq(lock);
  { //
    LazyExclusiveLock el(lock);
    ASSERT_TRUE(el);
    TrySharedLock sl(lock);
    ASSERT_FALSE(sl);
    assert_only_exclusive(lock);
  }
  assert_acq(lock);
}

TEST(ReadWriteLockTest, test_shared_prepare) {
  ReadWriteLock lock;
  assert_acq(lock);
  { //
    TrySharedLock sl(lock);
    ASSERT_TRUE(sl);
    assert_only_shared(lock);

    TryPrepareLock pl(lock);
    ASSERT_TRUE(pl);
    assert_only_shared_prepare(lock);

    TryPrepareLock pl2(lock);
    ASSERT_FALSE(pl2);
    assert_only_shared_prepare(lock);

    TrySharedLock sl2(lock);
    ASSERT_TRUE(sl2);
    assert_only_shared_prepare(lock);
  }
  assert_acq(lock);
  {
    TryPrepareLock pl(lock);
    ASSERT_TRUE(pl);
    assert_only_prepare(lock);

    TrySharedLock sl(lock);
    ASSERT_TRUE(sl);
    assert_only_shared_prepare(lock);

    TryPrepareLock pl2(lock);
    ASSERT_FALSE(pl2);
    assert_only_shared_prepare(lock);

    TrySharedLock sl2(lock);
    ASSERT_TRUE(sl2);
    assert_only_shared_prepare(lock);
  }
  assert_acq(lock);
}

TEST(ReadWriteLockTest, test_prepare_exclusive) {
  ReadWriteLock lock;
  assert_acq(lock);
  { //
    TryPrepareLock pl(lock);
    ASSERT_TRUE(pl);
    assert_only_prepare(lock);

    TryExclusiveLock el(lock);
    ASSERT_FALSE(el);
    assert_only_prepare(lock);

    TryExclusiveLock el2(pl);
    ASSERT_TRUE(el2);
    ASSERT_FALSE(pl);
    assert_only_exclusive(lock);

    TryPrepareLock pl2(lock);
    ASSERT_FALSE(pl2);
    assert_only_exclusive(lock);
  }
  assert_acq(lock);
  {
    TryExclusiveLock el(lock);
    ASSERT_TRUE(el);
    assert_only_exclusive(lock);

    TryPrepareLock pl(lock);
    ASSERT_FALSE(pl);
    assert_only_exclusive(lock);

    TryExclusiveLock el2(lock);
    ASSERT_FALSE(el2);
    assert_only_exclusive(lock);
  }
  assert_acq(lock);
  {
    LazyExclusiveLock el(lock);
    ASSERT_TRUE(el);
    assert_only_exclusive(lock);

    TryPrepareLock pl(lock);
    ASSERT_FALSE(pl);
    assert_only_exclusive(lock);

    TryExclusiveLock el2(lock);
    ASSERT_FALSE(el2);
    assert_only_exclusive(lock);
  }
  assert_acq(lock);
  {
    EagerExclusiveLock el(lock);
    ASSERT_TRUE(el);
    assert_only_exclusive(lock);

    TryPrepareLock pl(lock);
    ASSERT_FALSE(pl);
    assert_only_exclusive(lock);

    TryExclusiveLock el2(lock);
    ASSERT_FALSE(el2);
    assert_only_exclusive(lock);
  }
  assert_acq(lock);
}

TEST(ReadWriteLockTest, test_shared_prepare_exclusive) {
  ReadWriteLock lock;
  assert_acq(lock);
  { //
    TryPrepareLock pl(lock);
    ASSERT_TRUE(pl);
    assert_only_prepare(lock);

    {
      TrySharedLock sl0(lock);
      ASSERT_TRUE(sl0);
      assert_only_shared_prepare(lock);

      {
        TrySharedLock sl(lock);
        ASSERT_TRUE(sl);
        assert_only_shared_prepare(lock);

        TryExclusiveLock el(lock);
        ASSERT_FALSE(el);
        assert_only_shared_prepare(lock);

        TryExclusiveLock el2(pl);
        ASSERT_FALSE(el2);
        ASSERT_TRUE(pl);
        assert_only_shared_prepare(lock);
      }
      assert_only_shared_prepare(lock);
    }

    TryExclusiveLock el2(pl);
    ASSERT_TRUE(el2);
    ASSERT_FALSE(pl);
    assert_only_exclusive(lock);

    TryPrepareLock pl2(lock);
    ASSERT_FALSE(pl2);
    assert_only_exclusive(lock); //

    TrySharedLock sl(lock);
    ASSERT_FALSE(sl);
    assert_only_exclusive(lock); //
  }

  assert_acq(lock);
  { //
    {
      TrySharedLock sl0(lock);
      ASSERT_TRUE(sl0);
      assert_only_shared(lock); //

      {
        TryPrepareLock pl(lock);
        ASSERT_TRUE(pl);
        assert_only_shared_prepare(lock);

        {
          TrySharedLock sl(lock);
          ASSERT_TRUE(sl);
          assert_only_shared_prepare(lock);

          TryExclusiveLock el(lock);
          ASSERT_FALSE(el);
          assert_only_shared_prepare(lock);

          TryExclusiveLock el2(pl);
          ASSERT_FALSE(el2);
          ASSERT_TRUE(pl);
          assert_only_shared_prepare(lock);
        }
        TryExclusiveLock el2(pl);
        ASSERT_FALSE(el2);
        ASSERT_TRUE(pl);
        assert_only_shared_prepare(lock);
      }
      assert_only_shared(lock); //

      TryExclusiveLock el(lock);
      ASSERT_FALSE(el);
      assert_only_shared(lock); //
    }

    assert_acq(lock);
    TryExclusiveLock el2(lock);
    ASSERT_TRUE(el2);
    assert_only_exclusive(lock);
  }
  assert_acq(lock);
}
/*
 * ================================================
 * ===================THREAD=======================
 * ================================================
 */
struct ThreadedTestArg {
  sp::Barrier *b;
  sp::ReadWriteLock lock;
  size_t it;
  size_t toUpdate;
};

class ReadWriteLockThreadTest : public ::testing::TestWithParam<std::size_t> {};
INSTANTIATE_TEST_CASE_P(Default, ReadWriteLockThreadTest,
                        ::testing::Range(size_t(1), size_t(15)));

static void exclusive_test(size_t thCnt, void *(*worker)(void *)) {
  constexpr size_t thUpdate = 1024;
  const size_t final_update = thCnt * thUpdate;

  sp::Barrier b(thCnt);
  // printf("th: %zu\n", thCnt);

  pthread_t *ts = new pthread_t[thCnt];
  ThreadedTestArg arg;
  arg.b = &b;
  arg.it = thUpdate;
  arg.toUpdate = 0;

  for (size_t i(0); i < thCnt; ++i) {
    ts[i] = 0;
    int res = pthread_create(&ts[i], nullptr, worker, &arg);
    ASSERT_EQ(0, res);
    ASSERT_FALSE(ts[i] == 0);
  }

  for (size_t i(0); i < thCnt; ++i) {
    int res = pthread_join(ts[i], nullptr);
    ASSERT_EQ(0, res);
  }
  ASSERT_EQ(arg.toUpdate, final_update);

  delete[] ts;
}

//==================================================
static void *threaded_EagerExclusive(void *a) {
  auto arg = reinterpret_cast<ThreadedTestArg *>(a);
  arg->b->await();
  size_t i = 0;
  while (i < arg->it) {
    sp::EagerExclusiveLock guard(arg->lock);
    if (guard) {
      arg->toUpdate++;
      i++;
    }
  }
  return nullptr;
}

TEST_P(ReadWriteLockThreadTest, threaded_EagerExclusive) {
  const size_t thCnt = GetParam();
  exclusive_test(thCnt, threaded_EagerExclusive);
}

//==================================================
static void *threaded_TryPrepareEagerExclusive(void *a) {
  auto arg = reinterpret_cast<ThreadedTestArg *>(a);
  arg->b->await();
  size_t i = 0;
  while (i < arg->it) {
    sp::TryPrepareLock pre_guard(arg->lock);
    if (pre_guard) {
      assert_prepare(arg->lock);

      sp::EagerExclusiveLock ex_guard(pre_guard);
      if (ex_guard) {
        assert_only_exclusive(arg->lock);

        arg->toUpdate++;
        i++;
      }
    }
  }
  return nullptr;
}

TEST_P(ReadWriteLockThreadTest, threaded_TryPrepareEagerExclusive) {
  const size_t thCnt = GetParam();
  exclusive_test(thCnt, threaded_TryPrepareEagerExclusive);
}

//==================================================
static void *threaded_LazyExclusive(void *a) {
  auto arg = reinterpret_cast<ThreadedTestArg *>(a);
  arg->b->await();

  size_t i = 0;
  while (i < arg->it) {

    sp::LazyExclusiveLock guard(arg->lock);
    if (guard) {
      assert_only_exclusive(arg->lock);

      arg->toUpdate++;
      i++;
    }
  }
  return nullptr;
}

TEST_P(ReadWriteLockThreadTest, threaded_LazyExclusive) {
  const size_t thCnt = GetParam();
  exclusive_test(thCnt, threaded_LazyExclusive);
}

//==================================================
static void *threaded_TryExclusive(void *a) {
  auto arg = reinterpret_cast<ThreadedTestArg *>(a);
  arg->b->await();

  size_t i = 0;
  while (i < arg->it) {

    sp::TryExclusiveLock guard(arg->lock);
    if (guard) {
      assert_only_exclusive(arg->lock);

      arg->toUpdate++;
      i++;
    }
  }
  return nullptr;
}

TEST_P(ReadWriteLockThreadTest, threaded_TryExclusive) {
  const size_t thCnt = GetParam();
  exclusive_test(thCnt, threaded_TryExclusive);
}

// static void *threaded_TrySharedTryExclusive(void *a) {
//   auto arg = reinterpret_cast<ThreadedTestArg *>(a);
//   arg->b->await();
//   size_t i = 0;
//   while (i < arg->it) {
//     sp::TrySharedLock shared_guard(arg->lock);
//     if (shared_guard) {
//       sp::TryExclusiveLock ex_guard(shared_guard);
//       if (ex_guard) {
//         arg->toUpdate++;
//         i++;
//       }
//     }
//   }
//   return nullptr;
// }
//
// TEST_P(ReadWriteLockThreadTest, threaded_TrySharedTryExclusive) {
//   const size_t thCnt = GetParam();
//   exclusive_test(thCnt, threaded_TrySharedTryExclusive);
// }

//==================================================
static void *threaded_TryPrepareTryExclusive(void *a) {
  auto arg = reinterpret_cast<ThreadedTestArg *>(a);
  arg->b->await();
  size_t i = 0;
  while (i < arg->it) {
    sp::TryPrepareLock prep_guard(arg->lock);
    if (prep_guard) {
      assert_prepare(arg->lock);

      sp::TryExclusiveLock ex_guard(prep_guard);
      if (ex_guard) {
        assert_only_exclusive(arg->lock);

        arg->toUpdate++;
        i++;
      }
    }
  }
  return nullptr;
}

TEST_P(ReadWriteLockThreadTest, threaded_TryPrepareTryExclusive) {
  const size_t thCnt = GetParam();
  exclusive_test(thCnt, threaded_TryPrepareTryExclusive);
}

//==================================================
static void *threaded_TryPrepare(void *a) {
  auto arg = reinterpret_cast<ThreadedTestArg *>(a);
  arg->b->await();
  size_t i = 0;
  while (i < arg->it) {

    sp::TryPrepareLock prep_guard(arg->lock);
    if (prep_guard) {
      assert_prepare(arg->lock);

      arg->toUpdate++;
      i++;
    }
  }
  return nullptr;
}

TEST_P(ReadWriteLockThreadTest, threaded_TryPrepare) {
  const size_t thCnt = GetParam();
  exclusive_test(thCnt, threaded_TryPrepare);
}

//==================================================
static void *threaded_TrySharedTryPrepare(void *a) {
  auto arg = reinterpret_cast<ThreadedTestArg *>(a);
  arg->b->await();
  size_t i = 0;
  while (i < arg->it) {

    sp::TrySharedLock shared_guard(arg->lock);
    if (shared_guard) {
      assert_shared(arg->lock);

      sp::TryPrepareLock prep_guard(shared_guard);
      if (prep_guard) {
        assert_prepare(arg->lock);
        arg->toUpdate++;
        i++;
      }
    }
  }
  return nullptr;
}

TEST_P(ReadWriteLockThreadTest, threaded_TrySharedTryPrepare) {
  const size_t thCnt = GetParam();
  exclusive_test(thCnt, threaded_TrySharedTryPrepare);
}

//==================================================
static void *threaded_TrySharedTryPrepareEagerExclusive(void *a) {
  auto arg = reinterpret_cast<ThreadedTestArg *>(a);
  arg->b->await();
  size_t i = 0;
  while (i < arg->it) {
    sp::TrySharedLock shared_guard(arg->lock);
    if (shared_guard) {
      assert_shared(arg->lock);

      sp::TryPrepareLock prep_guard(shared_guard);
      if (prep_guard) {
        assert_prepare(arg->lock);

        sp::EagerExclusiveLock ex_guard(prep_guard);
        if (ex_guard) {
          assert_only_exclusive(arg->lock);

          arg->toUpdate++;
          i++;
        }
      }
    }
  }
  return nullptr;
}

template <std::size_t BS>
class Brute {
private:
  std::size_t buckets[BS];

public:
  Brute() //
      : buckets() {
  }

  bool next(std::size_t max) {
    return true;
  }
  const std::size_t *get() const {
    return buckets;
  }
};

//==================================================
TEST_P(ReadWriteLockThreadTest, threaded_TrySharedTryPrepareEagerExclusive) {
  const size_t thCnt = GetParam();
  exclusive_test(thCnt, threaded_TrySharedTryPrepareEagerExclusive);
}
// max:1[,,,,,]
// max:2[,,,,,]
static void tests(std::initializer_list<void *(*)(void *)> workers) {
  size_t workerThreads[100] = {1}; // TODO

  constexpr size_t thUpdate = 1024;

  const size_t thCnt = workers.size();
  const size_t final_update = thCnt * thUpdate;

  {
    sp::Barrier b(thCnt);

    std::vector<pthread_t> ts;
    ThreadedTestArg arg;
    arg.b = &b;
    arg.it = thUpdate;
    arg.toUpdate = 0;

    {
      size_t thW = 0;
      for (auto worker : workers) {
        size_t thCnt = workerThreads[thW];
        for (size_t th(0); th < thCnt; ++th) {
          ts.push_back(0);
          pthread_t &tid = ts.back();
          int res = pthread_create(&tid, nullptr, worker, &arg);
          ASSERT_EQ(0, res);
          ASSERT_FALSE(tid == 0);
        }
      }
    }

    for (auto &current : ts) {
      int res = pthread_join(current, nullptr);
      ASSERT_EQ(0, res);
    }
    ASSERT_EQ(arg.toUpdate, final_update);
  }
}

TEST(ReadWriteLockTest, threaded_combination) {
  // TODO combination of thread count for different work
  tests({threaded_TrySharedTryPrepareEagerExclusive,
         threaded_TryPrepareTryExclusive, threaded_TryExclusive,
         threaded_LazyExclusive, threaded_TryPrepareEagerExclusive,
         threaded_EagerExclusive});
}
