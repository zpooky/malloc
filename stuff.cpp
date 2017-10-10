#include "free.h"
#include "global.h"
#include "stuff.h"
#include "stuff_debug.h"
#include <atomic>
#include <cassert>

struct asd {
  // {
  sp::ReadWriteLock lock;
  std::atomic<local::PoolsRAII *> head;
  // }
  asd() noexcept
      : lock{}
      , head{nullptr} {
  }

  ~asd() {
    if (head.load()) {
      printf("memory leak\n");
    }
  }

  asd(const asd &) = delete;
  asd(const asd &&) = delete;
};

static asd internal_a;

static void
recycle_pool(asd &a, sp::SharedLock &lock, local::PoolsRAII *subject) noexcept {
  assert(subject);
  sp::TryPrepareLock pre_guard{lock};
  if (pre_guard) {
    local::PoolsRAII *const head = a.head.load();

    sp::EagerExclusiveLock ex_guard{pre_guard};
    if (ex_guard) {
      assert(subject->total_alloc.load() == 0);

      local::PoolsRAII *const priv = subject->priv;
      local::PoolsRAII *const next = subject->next;

      if (priv)
        priv->next = next;

      if (next)
        next->priv = priv;

      if (head == subject)
        a.head.store(next);

    } else {
      assert(false);
    }
  } else {
    // [1]
    // TODO maybe add /subject/ to Treiber stack. No ABA since the add is done
    // during the shared lock, and no entry can be deleted and re-enqueued
    // during a shared lock?
    assert(false);
  }
  subject->priv = nullptr;
  subject->next = nullptr;
}

static bool
enqueue(asd &a, sp::SharedLock &lock, local::PoolsRAII *subject) noexcept {
  assert(subject);
  assert(lock);

  sp::TryPrepareLock pre_guard{lock};
  if (pre_guard) {
    local::PoolsRAII *const head = a.head.load();

    sp::EagerExclusiveLock ex_guard{pre_guard};
    if (ex_guard) {
      subject->next = head;

      if (head)
        head->priv = subject;

      a.head.store(subject);

      return true;
    } else {
      assert(false);
    }
  }
  // TODO [1] but a dedicated stack for enqueue
  return false;
}

namespace global {

shared::FreeCode
free(void *const ptr) noexcept {
  using shared::FreeCode;

  sp::SharedLock shared_guard{internal_a.lock};
  if (shared_guard) {
    local::PoolsRAII *current = internal_a.head.load(std::memory_order_acquire);
  next:
    if (current) {
      const auto result = shared::free(*current, ptr);
      if (result != FreeCode::NOT_FOUND) {
        if (result == FreeCode::FREED_RECLAIM) {
          recycle_pool(internal_a, shared_guard, current);
        } else /*FreeCode::FREED*/ {
          // TODO enqueue frequently used Pool, but how to handle reference to
          // reclaimed pools?
        }

        return result;
      }
      current = current->next;
      goto next;
    }
  } else {
    assert(false);
  }

  return FreeCode::NOT_FOUND;
} // global::free()

util::maybe<std::size_t>
usable_size(void *const ptr) noexcept {
  sp::SharedLock shared_guard{internal_a.lock};
  if (shared_guard) {
    local::PoolsRAII *current = internal_a.head.load(std::memory_order_acquire);
  next:
    if (current) {
      auto result = shared::usable_size(*current, ptr);
      if (result) {
        return result;
      }
      current = current->next;
      goto next;
    }
  }
  return {};
}

util::maybe<void *>
realloc(void *const ptr, std::size_t length) noexcept {
  sp::SharedLock shared_guard{internal_a.lock};
  if (shared_guard) {
    local::PoolsRAII *current = internal_a.head.load(std::memory_order_acquire);
  next:
    if (current) {
      auto result = shared::realloc(*current, ptr, length);
      if (result) {
        return result;
      }
      current = current->next;
      goto next;
    }
  }
  return {};
}

local::PoolsRAII *
alloc_pool() noexcept {
  using PoolType = local::PoolsRAII;
  static_assert(sizeof(PoolType) <= SP_MALLOC_PAGE_SIZE * 2, "");

  PoolType *result = nullptr;
  void *const memory = global::alloc(SP_MALLOC_PAGE_SIZE * 2);
  if (memory) {
    result = new (memory) PoolType;

  retry:
    sp::SharedLock guard{internal_a.lock};
    if (guard) {
      if (!enqueue(internal_a, guard, result)) {
        printf("retry\n");
        goto retry;
      }
    }
  }

  return result;
} // global::alloc_pool()

void
release_pool(local::PoolsRAII *pool) noexcept {
  pool->reclaim.store(true);
  std::size_t allocs = pool->total_alloc.load();
  if (allocs == 0) {
    sp::SharedLock lock{internal_a.lock};
    if (lock) {
      recycle_pool(internal_a, lock, pool);
    }
  }
} // global::release_pool()

} // namespace stuff

#ifdef SP_TEST

namespace debug {

std::size_t
count_unclaimed_pools() noexcept {
  std::size_t result = 0;
  sp::SharedLock shared_guard{internal_a.lock};
  if (shared_guard) {
    local::PoolsRAII *current = internal_a.head.load(std::memory_order_acquire);
  next:
    if (current) {
      result++;
      current = current->next;
      goto next;
    }
  }
  return result;
} // debug::count_unclaimed_pools()

} // namespace debug
#endif
