#include "free.h"
#include "global.h"
#include "stuff.h"
#ifdef SP_TEST
#include "alloc_debug.h"
#include "stuff_debug.h"
#endif
#include <atomic>
#include <cassert>
#include <cstring>

#define SP_MALLOC_POOL_SIZE sp::node_size(SP_MALLOC_PAGE_SIZE * 2)

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

static bool
unlink_pool(asd &a, sp::SharedLock &lock, local::PoolsRAII *subject) noexcept {
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
    // assert(false);
    return false;
  }
  subject->priv = nullptr;
  subject->next = nullptr;
  return true;
}

static bool
recycle_pool(asd &a, sp::SharedLock &lock, local::PoolsRAII *subject) noexcept {
  if (unlink_pool(a, lock, subject)) {
    void *const target = reinterpret_cast<void *>(subject);
    constexpr std::size_t length(SP_MALLOC_POOL_SIZE);
    std::memset(target, 0, length);
    global::dealloc(target, sp::node_size(length));
    return true;
  }
  return false;
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
          // printf("free()=FREED_RECLAIM\n");
          if (!recycle_pool(internal_a, shared_guard, current)) {
            // TODO !!! FIX does not work
            assert(false);
          }
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

util::maybe<sp::bucket_size>
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
realloc(local::PoolsRAII &tl, void *const ptr, std::size_t length) noexcept {
  sp::SharedLock shared_guard{internal_a.lock};
  if (shared_guard) {
    local::PoolsRAII *current = internal_a.head.load(std::memory_order_acquire);
  next:
    if (current) {
      auto code = shared::FreeCode::FREED;
      auto result = shared::realloc(tl, *current, ptr, length, /*OUT*/ code);
      if (code == shared::FreeCode::FREED_RECLAIM) {
        if (!recycle_pool(internal_a, shared_guard, current)) {
          // TODO
          assert(false);
        }
      }
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
realloc(local::Pools &tl, void *const ptr, std::size_t length) noexcept {
  assert(tl.pools);
  return realloc(*tl.pools, ptr, length);
}

local::PoolsRAII *
alloc_pool() noexcept {
  using PoolType = local::PoolsRAII;

  constexpr std::size_t length(SP_MALLOC_POOL_SIZE);
  static_assert(sizeof(PoolType) <= length, "");

  PoolType *result = nullptr;
  void *const memory = global::alloc(sp::node_size(length));
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
  retry:
    sp::SharedLock lock{internal_a.lock};
    if (lock) {
      if (!recycle_pool(internal_a, lock, pool)) {
        goto retry;
      }
    }
  }
} // global::release_pool()

} // namespace global

#ifdef SP_TEST

namespace debug {

std::size_t
stuff_count_unclaimed_orphan_pools() noexcept {
  std::size_t result = 0;
  sp::SharedLock shared_guard{internal_a.lock};
  if (shared_guard) {
    local::PoolsRAII *current = internal_a.head.load(std::memory_order_acquire);
  next:
    if (current) {
      if (current->reclaim.load()) {
        result++;
      }
      current = current->next;
      goto next;
    }
  }
  return result;
} // debug::count_unclaimed_pools()

std::size_t
stuff_count_alloc() {
  std::size_t result = 0;
  sp::SharedLock shared_guard{internal_a.lock};
  if (shared_guard) {
    local::PoolsRAII *current = internal_a.head.load(std::memory_order_acquire);
  next:
    if (current) {
      result += alloc_count_alloc(*current);
      current = current->next;
      goto next;
    }
  }
  return result;
} // debug::stuff_count_alloc()

std::size_t
stuff_count_alloc(std::size_t sz) {
  std::size_t result = 0;
  sp::SharedLock shared_guard{internal_a.lock};
  if (shared_guard) {
    local::PoolsRAII *current = internal_a.head.load(std::memory_order_acquire);
  next:
    if (current) {
      result += alloc_count_alloc(*current, sz);
      current = current->next;
      goto next;
    }
  }
  return result;
} // debug::stuff_count_alloc()

void
stuff_force_reclaim_orphan() {
  sp::SharedLock lock{internal_a.lock};
  if (lock) {
    local::PoolsRAII *current = internal_a.head.load(std::memory_order_acquire);
  next:
    if (current) {
      if (current->reclaim.load()) {
        // TODO reclaim extents
        recycle_pool(internal_a, lock, current);
      }
      current = current->next;
      goto next;
    }
  }
} // debug::stuff_force_reclaim()

} // namespace debug
#endif
