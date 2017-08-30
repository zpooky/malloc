#include "ReadWriteLock.h"
#include "gtest/gtest.h"
#include <list>

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
