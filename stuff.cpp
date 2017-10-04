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
};

static asd a;

namespace global {

bool
free(void *ptr) noexcept {
  sp::SharedLock guard{a.lock};
  if (guard) {
    local::PoolsRAII *current = a.head.load(std::memory_order_acquire);
  next:
    if (current) {
      if (shared::free(*current, ptr)) {
        // TODO enqueue frequently used Pool
        return true;
      }
      current = current->global.load(std::memory_order_acquire);
      goto next;
    }
  }
  return false;
} // free()

local::PoolsRAII *
alloc_pool() noexcept {
  using PoolType = local::PoolsRAII;
  static_assert(sizeof(PoolType) <= SP_MALLOC_PAGE_SIZE * 2, "");

  PoolType *result = nullptr;
  void *const memory = global::alloc(SP_MALLOC_PAGE_SIZE * 2);
  if (memory) {
    result = new (memory) PoolType;

    sp::SharedLock guard{a.lock};
    if (guard) {
      auto compare = a.head.load(std::memory_order_acquire);
    retry:
      result->global.store(compare, std::memory_order_release);
      if (!a.head.compare_exchange_strong(compare, result)) {
        goto retry;
      }
    }
  }

  return result;
} // alloc_pool()

void
release_pool(local::PoolsRAII *pool) noexcept {
  // 1. relase local::free_list to global::free_list
  pool->reclaim.store(true);
} // release_pool()

} // namespace stuff
