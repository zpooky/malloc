#include "global.h"

#include "ReadWriteLock.h"
#include <cassert>
#include <mutex>
#include <tuple>

#include <unistd.h> //sbrk

namespace {
struct State {
  // brk
  // {{{
  std::mutex brk_lock;
  void *brk_position; // not used for now
  std::size_t brk_alloc;
  // }}}

  // free
  // {{{
  header::Free free;
  // }}}

  State() noexcept                                       //
      : brk_lock{}, brk_position{nullptr}, brk_alloc{0}, //
        free{0, nullptr} {
    std::atomic_thread_fence(std::memory_order_release);
  }
};

static State state;

header::Free *find_free(size_t size) noexcept {
  header::Free *head = &state.free;
retry : {
  // lock access to next
  // TODO TrySharedLock
  sp::SharedLock guard(head->next_lock);
  if (guard) {
    header::Free *current = head->next.load(std::memory_order_relaxed);
    if (current) {
      if (current->size < size) {
        //TRyExcLock
        head = current;
        goto retry;
      }
      // free_deque()
    }
  } else {
    //TODO some other thread has exclusive lock on node
  }
}
  return nullptr;
}

header::Free *alloc_free(size_t atLeast) noexcept {
  void *res = nullptr;
  {
    std::lock_guard<std::mutex> guard(state.brk_lock);
    // if (state.brk_position == nullptr) {
    //   state.brk_position = ::sbrk(0);
    // }
    // TODO some algorithm to determine optimal alloc size
    std::size_t allocSz = std::max(state.brk_alloc, SP_ALLOC_INITIAL_ALLOC);
    allocSz = std::max(atLeast, allocSz);
    // TODO check size wrap around

    // void *newPos = state.brk_position + allocSz;
    res = ::sbrk(allocSz);
    if (res != (void *)-1) {
      state.brk_alloc = state.brk_alloc + allocSz;
      return header::init_free(res, allocSz, nullptr);
    }
  }

  return nullptr;
}

void return_free(header::Free *free) noexcept {
  if (free) {
    sp::SharedLock guard(state.free.next_lock);
    header::Free *next = state.free.next.load(std::memory_order_acquire);
  retry:
    if (!state.free.next.compare_exchange_strong(next, free)) {
      goto retry;
    }
  }
}

void return_free(void *const ptr, size_t length) noexcept {
  return return_free(header::init_free(ptr, length, nullptr));
}

} // namespace

/*
 *===========================================================
 *=======GLOBAL==============================================
 *===========================================================
 */
namespace global {

// TODO change so it should be number of pages instead of a specific
// length+alignment
std::tuple<void *, std::size_t> alloc(std::size_t p_length) noexcept {
  header::Free *free = find_free(p_length);
  if (free == nullptr) {
    free = alloc_free(p_length);
    if (free == nullptr) {
      return std::make_tuple(nullptr, std::size_t(0));
    }
  }

  void *const unalign_ptr = reinterpret_cast<void *>(free);
  void *const align_ptr = util::align_pointer(unalign_ptr, 8);
  ptrdiff_t unalign_length = util::ptr_diff(align_ptr, unalign_ptr);
  std::size_t align_length = free->size - unalign_length;

  if (align_length > p_length) {
    if (align_ptr != unalign_ptr) {
      return_free(unalign_ptr, unalign_length);
      assert(false);
    }

    return_free(util::ptr_math(align_ptr, +p_length), align_length - p_length);
    return std::make_tuple(align_ptr, p_length);
  }

  return_free(free);
  assert(false);
  return std::make_tuple(nullptr, std::size_t(0));
} // alloc()

bool free(void *const) noexcept {
  // TODO
  return true;
} // free()

local::PoolsRAII *alloc_pool() noexcept {
  using PoolType = local::PoolsRAII;
  static_assert(sizeof(PoolType) <= SP_MALLOC_PAGE_SIZE, "");
  auto result = alloc(SP_MALLOC_PAGE_SIZE);

  void *start = std::get<0>(result);
  if (start == nullptr) {
    assert(false);
  }
  return new (start) PoolType;
} // alloc_pool()

void release_pool(local::PoolsRAII *) noexcept {
  // TODO pool is of size SP_MALLOC_PAGE_SIZE
} // release_pool()

} // namespace global
