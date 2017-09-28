#include "global.h"
#include "stuff.h"
#include <atomic>
#include <cassert>

struct asd {
  sp::ReadWriteLock lock;
  std::atomic<local::PoolsRAII *> head;
  asd() noexcept
      : lock{}
      , head{nullptr} {
  }
};

static asd a;

namespace stuff {

bool
free(void *) noexcept {
  // TODO
  return false;
} // free()

local::PoolsRAII *
alloc_pool() noexcept {
  using PoolType = local::PoolsRAII;
  static_assert(sizeof(PoolType) <= SP_MALLOC_PAGE_SIZE, "");

  PoolType *result = nullptr;
  void *const memory = global::alloc(SP_MALLOC_PAGE_SIZE);
  if (memory) {
    result = new (memory) PoolType;

    sp::SharedLock guard{a.lock};
    if (guard) {
      auto compare = a.head.load(std::memory_order_acquire);
    retry:
      if (!a.head.compare_exchange_strong(compare, result)) {
        goto retry;
      }
    }
  }

  return result;
} // alloc_pool()

void
release_pool(local::PoolsRAII *) noexcept {
  // TODO pool is of size SP_MALLOC_PAGE_SIZE
} // release_pool()

} // namespace stuff
