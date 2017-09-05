#include "global.h"

#include "ReadWriteLock.h"
#include <cassert>
#include <cstdio>
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

void free_dequeue(header::Free &head, header::Free &target) noexcept {
  assert(&head != &target);
  head.next.store(target.next.load());
  target.next.store(nullptr);
}

void free_enqueue(header::Free *const head,
                  header::Free *const target) noexcept {
  assert(head != nullptr);
  assert(target != nullptr);
  assert(head != target);

  if (header::is_consecutive(*head, *target)) {
    header::coalesce(*head, *target);
  } else {
    target->next.store(head->next.load());
    head->next.store(target);
  }
}

header::Free *find_free(size_t size) noexcept {
start:
  header::Free *head = &state.free;
retry:
  if (true) {
    sp::TrySharedLock shGuard(head->next_lock);
    if (shGuard) {
      header::Free *current = head->next.load(std::memory_order_relaxed);
      if (current) {
        if (size <= current->size) {
          // matching
          sp::TryPrepareLock preGuard(shGuard);
          if (preGuard) {
            // TODO maybe TryPrepare(current->next_lock) is enough here
            sp::TryExclusiveLock headGuard(current->next_lock);
            if (headGuard) {
              sp::TryExclusiveLock exGuard(preGuard);
              if (exGuard) {
                free_dequeue(*head, *current);
                return current;
              } else {
                // bug
                assert(false);
              }
            } else {
              // Could not lock current->next
              head = current;
              assert(false); // TODO just for test
              goto retry;
            }
          } else {
            // Some other thread got here first, continue
            head = current;
            assert(false); // TODO just for test
            goto retry;
          }
        } else {
          // Not matching, continue
          head = current;
          goto retry;
        }
      } else {
        // Not found any matching Free node
        return nullptr;
      }
    } else {
      // TODO some other thread has exclusive lock on node, restart?
      goto start;
    }
  }
  return nullptr;
}

header::Free *alloc_free(const size_t atLeast) noexcept {
  // #ifdef SP_MALLOC_TEST_NO_ALLOC
  assert(false);
  exit(123);
  // #endif

  {
    std::lock_guard<std::mutex> guard(state.brk_lock);
    // TODO some algorithm to determine optimal alloc size
    std::size_t allocSz(0);
    allocSz = std::max(state.brk_alloc, SP_ALLOC_INITIAL_ALLOC);
    allocSz = std::max(atLeast, allocSz);
  // TODO check size wrap around
  retry:
    void *const res = ::sbrk(allocSz);
    if (res != (void *)-1) {
      state.brk_alloc = state.brk_alloc + allocSz;
      return header::init_free(res, allocSz, nullptr);
    } else if (allocSz > atLeast) {
      allocSz = allocSz / 2;
      allocSz = std::max(allocSz, atLeast);
      goto retry;
    }
  }

  return nullptr;
}

void return_free(header::Free *const toReturn) noexcept {
  // [Head]->[Current]->[Next]
  if (toReturn) {
  start:
    header::Free *head = &state.free;
    sp::TrySharedLock cur_shared_guard(head->next_lock);
    if (!cur_shared_guard) {
      //...
      goto start;
    }
  retry:
    if (true) {
      // [Current:SHARED][Next:-]

      sp::TryPrepareLock cur_pre_guard(cur_shared_guard);
      if (cur_pre_guard) {
        // [Current:PREPARE][Next:-]

        header::Free *const current =
            head->next.load(std::memory_order_relaxed);
        if (current) { //<--------------------
          /*
           *
           */
          if (toReturn > current) {

            // TODO TryPrepare guard is probably enough here
            sp::TryExclusiveLock next_exc_guard(current->next_lock);
            if (next_exc_guard) {
              // [Current:PREPARE][Next:EXCLUSIVE]

              sp::TryExclusiveLock cur_exc_guard(cur_pre_guard);
              if (cur_exc_guard) {
                // [Current:EXCLUSIVE][Next:EXCLUSIVE]

                free_enqueue(current, toReturn);
                return;
              } /*Current Exclusive Guard*/ else {
                // bug - if PREPARE then a exclusive should always succeed?
                assert(false);
              }
            } /*Next Exclusive Guard*/ else {
              //...???
              goto start;
            }
          }
          /*
           *
           */
          sp::TrySharedLock next_shared_guard(current->next_lock);
          if (next_shared_guard) {
            cur_shared_guard.swap(next_shared_guard);
            head = current;
            goto retry;
          } else {
            //...?
            goto start;
          }
          //<---------------------------
        } /*current*/ else {
          // current is null
          free_enqueue(head, toReturn);
          return;
        }
      } /*Current Prepare Guard*/ else {
        assert(false); // TODO
      }
    }
    assert(false); // leak memory here
  }
}

void return_free(void *const ptr, size_t length) noexcept {
  // assert(ptr != nullptr);
  header::Free *const toReturn = header::init_free(ptr, length, nullptr);
  if (toReturn) {
    assert(ptr == toReturn);
    return return_free(toReturn);
  }
}

} // namespace

#ifdef SP_TEST
namespace test { //
std::vector<std::tuple<void *, std::size_t>> watch_free() {
  std::vector<std::tuple<void *, std::size_t>> result;
  header::Free *head = state.free.next.load();
start:
  if (head) {
    result.emplace_back(head, head->size);
    head = head->next.load(std::memory_order_acquire);
    // printf("%zu-%p,%zu\n", ++i, reinterpret_cast<void *>(head), head->size);
    goto start;
  }

  return result;
}
void clear_free() {
  state.free.next.store(nullptr);
}

void print_free() {
  header::Free *head = state.free.next.load();
  if (head) {
    printf("cmpar: ");
    header::debug_print_free(head);
  }
}

} // namespace test
#endif

/*
 *===========================================================
 *=======GLOBAL==============================================
 *===========================================================
 */
namespace global {

// TODO change so it should be number of pages instead of a specific
// length+alignment
void *alloc(std::size_t p_length) noexcept {
  header::Free *free = find_free(p_length);
  if (free == nullptr) {
    free = alloc_free(p_length);
    if (free == nullptr) {
      return nullptr;
    }
  }

  void *const unalign_ptr = reinterpret_cast<void *>(free);
  void *const align_ptr = util::align_pointer(unalign_ptr, 8);
  ptrdiff_t unalign_length = util::ptr_diff(align_ptr, unalign_ptr);
  std::size_t align_length = free->size - unalign_length;

  if (align_length >= p_length) {
    if (align_ptr != unalign_ptr) {
      return_free(unalign_ptr, unalign_length);
      assert(false);
    }

    void *const retFree = util::ptr_math(align_ptr, +p_length);
    const size_t retLength = align_length - p_length;
    return_free(retFree, retLength);
    return align_ptr;
  }

  return_free(free);
  assert(false);
  return nullptr;
} // alloc()

void dealloc(void *const start, std::size_t length) noexcept {
  return return_free(start, length);
}

bool free(void *const) noexcept {
  // TODO
  return true;
} // free()

local::PoolsRAII *alloc_pool() noexcept {
  using PoolType = local::PoolsRAII;
  static_assert(sizeof(PoolType) <= SP_MALLOC_PAGE_SIZE, "");
  void *result = alloc(SP_MALLOC_PAGE_SIZE);

  if (result == nullptr) {
    assert(false);
  }
  return new (result) PoolType;
} // alloc_pool()

void release_pool(local::PoolsRAII *) noexcept {
  // TODO pool is of size SP_MALLOC_PAGE_SIZE
} // release_pool()

} // namespace global
