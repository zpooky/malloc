#include "global.h"

#include "ReadWriteLock.h"
#include <cassert>
#include <cstdio>
#include <mutex>

#ifdef SP_TEST
#include "global_debug.h"
#endif

#include <unistd.h> //sbrk

namespace {

static global::State internal_state;

static void
free_dequeue(header::Free *head, header::Free *target) noexcept {
  assert(head != target);
  head->next.store(target->next.load());
  target->next.store(nullptr);
} // ::free_dequeue()

static void
free_enqueue(header::Free *head, header::Free *target) noexcept {
  assert(head);
  assert(target);
  assert(head != target);

  if (header::is_consecutive(head, target)) {
    header::Free *const next = head->next.load(std::memory_order_relaxed);
    header::coalesce(head, target, next);
  } else {
    target->next.store(head->next.load());
    head->next.store(target);
  }
} // ::free_enqueue()

header::Free *
find_free(global::State &state, sp::node_size size) noexcept {
start:
  if (true) {
    header::Free *head = &state.free;
    sp::TrySharedLock cur_shared_guard(head->next_lock);
    if (!cur_shared_guard) {
      //...
      goto start;
    }
  retry:
    if (true) {
      // [Current:SHARED][Next:-]

      header::Free *const current = head->next;
      if (current) {

        sp::TrySharedLock next_shared_guard(current->next_lock);
        if (next_shared_guard) {
          if (size <= current->size) {
            // matching

            sp::TryPrepareLock cur_pre_guard(cur_shared_guard);
            if (cur_pre_guard) {
              // [Current:PREPARE][Next:-]

              sp::TryPrepareLock next_pre_guard(next_shared_guard);
              if (next_pre_guard) {

                sp::EagerExclusiveLock cur_exc_guard(cur_pre_guard);
                sp::EagerExclusiveLock next_exc_guard(next_pre_guard);
                if (cur_exc_guard && next_exc_guard) {
                  {
                    assert(head->next_lock.has_exclusive_lock());
                    assert(head->next_lock.shared_locks() == 0);
                    assert(!head->next_lock.has_prepare_lock());

                    assert(current->next_lock.has_exclusive_lock());
                    assert(current->next_lock.shared_locks() == 0);
                    assert(!current->next_lock.has_prepare_lock());
                  }

                  if (size == current->size ||
                      (size + sizeof(header::Free)) > current->size) {
                    // printf("free_dequeue(head,
                    // current)size:%zu,cur-sz:%zu\n",
                    //        size, current->size);
                    free_dequeue(head, current);
                    return current;
                  } else {
                    // printf("header::reduce(current,
                    // size)size:%zu,cur-sz:%zu\n",
                    //        size, current->size);
                    return header::reduce(current, size);
                  }
                } /*cur_exc_guard*/ else {
                  // bug
                  assert(false);
                  return nullptr;
                }
              } /*next_pre_guard*/ else {
                // Could not lock current->next
                cur_shared_guard.swap(next_shared_guard);
                head = current;
                goto retry;
              }
            } /*cur_pre_guard*/ else {
              // Some other thread got here first, continue
              cur_shared_guard.swap(next_shared_guard);
              head = current;
              goto retry;
            }
          } else {
            // Not matching, continue
            cur_shared_guard.swap(next_shared_guard);
            head = current;
            goto retry;
          }
        } /*next_shared_guard*/ else {
          goto start; //???
        }
      } else {
        // Not found any matching Free node
        return nullptr;
      }
    }
  }
  return nullptr;
} // ::find_free()

header::Free *
alloc_free(global::State &state, sp::node_size atLeast) noexcept {
  {
    std::lock_guard<std::mutex> guard(state.brk_lock);
    // TODO some algorithm to determine optimal alloc size
    sp::node_size allocSz(0);
    allocSz = std::max(state.brk_alloc, std::size_t(SP_ALLOC_INITIAL_ALLOC));
    allocSz = std::max(atLeast, allocSz);
  // TODO check size wrap around
  retry:
    void *const res = ::sbrk(std::size_t(allocSz));
    if (res != (void *)-1) {
      state.brk_alloc += std::size_t(allocSz);
      return header::init_free(res, allocSz);
    } else if (allocSz > atLeast) {
      allocSz = allocSz / 2;
      allocSz = std::max(allocSz, atLeast);
      goto retry;
    }
  }

  return nullptr;
} // ::alloc_free()

void
return_free(global::State &state, header::Free *const toReturn) noexcept {
// [Head]->[Current]->[Next]
start:
  if (true) {
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

        header::Free *const current = head->next;
        if (current) { //<--------------------
          if (toReturn > current) {

            sp::TrySharedLock next_shared_guard(current->next_lock);
            if (next_shared_guard) {
              // [Current:PREPARE][Next:SHARED]

              if (toReturn > current->next.load()) {
                // higher than next aswell
                cur_shared_guard.swap(next_shared_guard);
                head = current;
                goto retry;
              }

              // TODO should succeed if prepare:0 & // exc:0
              sp::TryPrepareLock next_pre_guard(next_shared_guard);
              if (next_pre_guard) {
                // [Current:PREPARE][Next:EXCLUSIVE]

                // TODO cur_lock shoud unblockingly set the exclusive flag and
                // continue to let next_lock execute in paralell first when we
                // check for success. In the if case we should block until
                // exclusive lock. Applies to both return & alloc free
                sp::EagerExclusiveLock cur_exc_guard(cur_pre_guard);
                sp::EagerExclusiveLock next_exc_guard(next_pre_guard);
                if (cur_exc_guard && next_exc_guard) {
                  // [Current:EXCLUSIVE][Next:EXCLUSIVE]
                  {
                    assert(head->next_lock.has_exclusive_lock());
                    assert(head->next_lock.shared_locks() == 0);
                    assert(!head->next_lock.has_prepare_lock());

                    assert(current->next_lock.has_exclusive_lock());
                    assert(current->next_lock.shared_locks() == 0);
                    assert(!current->next_lock.has_prepare_lock());
                  }

                  free_enqueue(current, toReturn);

                  // header::Free *const next = current->next.load();
                  // if (header::is_consecutive(current, next)) {
                  // //?
                  // header::Free *const next_next =
                  //     next->next.load(std::memory_order_acquire);
                  // header::coalesce(current, next, next_next);
                  // }
                  return;
                } /*Current Exclusive Guard*/ else {
                  // bug - TODO if PREPARE then a exclusive should always
                  // succeed?
                  assert(false);
                  return;
                }
              } /*Next Exclusive Guard*/ else {
                //...???
                goto start;
              }
            } /*Next Shared Guard*/ else {
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
          } /*next_shared_guard*/ else {
            //...?
            goto start;
          }
          //<---------------------------
        } /*current*/ else {
          // current is null
          free_enqueue(head, toReturn);
          return;
        }
      } /*current_pre_guard*/ else {
        // assert(false); // TODO fails
        goto start;
      }
    }
    assert(false); // leak memory here
  }
} // ::return_free()

void
return_free(global::State &s, void *const ptr, sp::node_size length) noexcept {
  header::Free *const toReturn = header::init_free(ptr, length);
  if (toReturn) {
    assert(ptr == toReturn);
    return return_free(s, toReturn);
  }
} // ::return_free()

} // namespace

/*
 *===========================================================
 *=======DEBUG===============================================
 *===========================================================
 */
#ifdef SP_TEST
namespace debug { //
static void
debug_print_free(header::Free *const head) {
  if (head) {
    void *t = reinterpret_cast<void *>(head);
    printf("[%p,%zu]", t, std::size_t(head->size));
    debug_print_free(head->next.load());
  } else {
    printf("\n");
  }
} // test::debug_print_free()

std::vector<std::tuple<void *, std::size_t>>
global_get_free(global::State *state) {
  if (state == nullptr) {
    state = &internal_state;
  }
  std::vector<std::tuple<void *, std::size_t>> result;
  header::Free *head = state->free.next.load();
start:
  if (head) {
    result.emplace_back(head, head->size);
    head = head->next.load(std::memory_order_acquire);
    // printf("%zu-%p,%zu\n", ++i, reinterpret_cast<void *>(head), head->size);
    goto start;
  }

  return result;
} // test::watch_free()

void
clear_free(global::State *state) {
  if (state == nullptr) {
    state = &internal_state;
  }
  state->free.next.store(nullptr);
} // test::clear_free()

void
print_free(global::State *state) {
  if (state == nullptr) {
    state = &internal_state;
  }
  header::Free *head = state->free.next.load(std::memory_order_acquire);
  if (head) {
    printf("cmpar: ");
    debug_print_free(head);
  }
} // test::print_free()

std::size_t
count_free(global::State *state) {
  if (state == nullptr) {
    state = &internal_state;
  }
  std::size_t result = 0;
  header::Free *head = state->free.next.load();
start:
  if (head) {
    result++;
    head = head->next.load(std::memory_order_acquire);
    goto start;
  }
  return result;
} // test::count_free()

static void
swap(header::Free *p, header::Free *c, header::Free *n) {
  p->next.store(n);
  c->next.store(n->next);
  n->next.store(c);
} // swap()

void
sort_free(global::State *state) {
  // TODO
  if (state == nullptr) {
    state = &internal_state;
  }
restart:
  bool swapped = false;
  header::Free *priv = &state->free;
  header::Free *current = priv->next.load();
start:
  if (current) {
    header::Free *const next = current->next;
    if (next) {
      if (next > current) {
        swap(priv, current, next);
        swapped = true;
        priv = next;
        // current = current;
        goto restart;
      } else {
        priv = current;
        current = next;
      }
      goto start;
    } else if (swapped) {
      goto restart;
    }
  } else if (swapped) {
    goto restart;
  }
} // test::sort_free()

void
coalesce_free(global::State *state) {
  if (state == nullptr) {
    state = &internal_state;
  }
  // sort_free(state);
  header::Free *head = state->free.next.load();
start:
  if (head) {
  get_next:
    header::Free *const next = head->next;
    if (next) {
      if (header::is_consecutive(head, next)) {
        header::coalesce(head, next, next->next);
        goto get_next;
      }
      head = next;
      goto start;
    }
  }
} // test::coalesce_free()

} // namespace test
#endif

/*
 *===========================================================
 *=======GLOBAL==============================================
 *===========================================================
 */
namespace global {
namespace internal {

header::Free *
find_freex(State &state, sp::node_size length) noexcept {
  return find_free(state, length);
}

void *
alloc(State &state, sp::node_size p_length) noexcept {
  if (p_length == 0) {
    return nullptr;
  }

  header::Free *free = find_freex(state, p_length);
  if (free == nullptr) {
    free = alloc_free(state, p_length);
    if (free == nullptr) {
      return nullptr;
    }
  }

  void *const unalign_ptr = reinterpret_cast<void *>(free);
  void *const align_ptr = util::align_pointer(unalign_ptr, 8);
  ptrdiff_t unalign_length = util::ptr_diff(align_ptr, unalign_ptr);
  sp::node_size align_length = free->size - unalign_length;

  if (align_length >= p_length) {
    if (align_ptr != unalign_ptr) {
      return_free(state, unalign_ptr, sp::node_size(unalign_length));
      assert(false);
    }

    void *const retFree = util::ptr_math(align_ptr, +std::size_t(p_length));
    const sp::node_size retLength(align_length - p_length);
    return_free(state, retFree, retLength);
    return align_ptr;
  }

  return_free(state, free);
  assert(false);
  return nullptr;
}

void
dealloc(State &state, void *const start, sp::node_size length) noexcept {
  assert(start);
  return return_free(state, start, length);
} // global::internal::dealloc()

void
dealloc(State &state, header::LocalFree *current) noexcept {
  assert(free);
// TODO make better
next:
  if (current) {
    header::LocalFree *next = current->next;
    dealloc(state, current, current->size);
    current = next;
    goto next;
  }
} // global::internal::dealloc()

} // namespace internal

// TODO change so it should be number of pages instead of a specific
// length+alignment
void *
alloc(sp::node_size length) noexcept {
  return internal::alloc(internal_state, length);
} // global::alloc()

void
dealloc(void *const start, sp::node_size length) noexcept {
  return internal::dealloc(internal_state, start, length);
} // global::dealloc()

void
dealloc(header::LocalFree *const start) noexcept {
  return internal::dealloc(internal_state, start);
} // global::dealloc()

} // namespace global
