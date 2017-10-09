#include "free.h"
#include "global.h"
#include "stuff.h"
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

bool
free(void *const ptr) noexcept {
  sp::SharedLock shared_guard{internal_a.lock};
  if (shared_guard) {
    local::PoolsRAII *current = internal_a.head.load(std::memory_order_acquire);
  next:
    if (current) {
      const auto result = shared::free(*current, ptr);
      if (result != shared::FreeCode::NOT_FOUND) {
        if (result == shared::FreeCode::FREED_RECLAIM) {
          recycle_pool(internal_a, shared_guard, current);
        } else /*FreeCode::FREED*/ {
          // TODO enqueue frequently used Pool, but how to handle reference to
          // reclaimed pools?
        }

        return true;
      }
      current = current->next;
      goto next;
    }
  } else {
    assert(false);
  }

  return false;
} // global::free()

std::size_t
usable_size(void *const ptr) noexcept {
  sp::SharedLock shared_guard{internal_a.lock};
  if (shared_guard) {
    local::PoolsRAII *current = internal_a.head.load(std::memory_order_acquire);
  next:
    if (current) {
      std::size_t result = shared::usable_size(*current, ptr);
      if (result != 0) {
        return result;
      }
      current = current->next;
      goto next;
    }
  }
  return 0;
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
