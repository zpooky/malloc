#include <algorithm>
#include <cassert>

#ifdef SP_TEST
#include "malloc_debug.h"
#endif

#include "alloc.h"
#include "free.h"
#include "malloc.h"
#include "shared.h"
#include "stuff.h"

// http://preshing.com/20131125/acquire-and-release-fences-dont-work-the-way-youd-expect/

// # version 2.0
// - from a pointer get the node header and in the node header get a pointer to
// the extent header to support random access free(). Maybe have a tag in the
// node header like Thread id to make global::free easier.
// `node::Header header_for(ptr);`

// - use memmap instead of sbrk to more freely allow for reclamation of
// large unused ranges of global free-list memory. Without being blocked by a
// single high sbrk reservation.

// sp::RefCounter Pools in global

// # TODO
// 1. local::free_list
//  * best fit
//  * local::malloc coalesce local free list
//  * only single mallocing thread is allowed the dequeuing
//  * multiple free threads is allowed to enqueue
//  * high & low watermark on free list size based on total allocated by thread
//  * global::alloc(minimum, desired) desired size is optional to fulfil
//  * local -> global free list reclamation based on high watermark
//
// 2. Local pool reclamation from global
//  * Non-thread local free()
//  * detect if all pooled memory is reclaimed
//  * counter in Pools of how much memory is present in the different pool
//    * does not include local::free list
//    * increase on local::alloc(Node/Extent)
//    * decrease on local::dealloc(Node/Extent)
//  * global will look in local::Pool reclamation before sbrk?
//  * on g::dealloc_pool() will check if Pools::size is 0 if so directly reclaim
//  * Shared local::Polls lock used by free from other threads not local
//    malloc&free
//
// 3. local::free_list free reclamation
//  * logic for reclaim on TL free the Extent
//  * logic for reclaim on non-TL free the Extent
//
// 5. global::free_list
//  *  (shrinking/reclamation/release/dealloc) reclaim usuable mem to sbrk
//  * change interface for global::alloc to N 4096 pages not X bytes
//
// 9. global::free_list
//  * how to better coalesce global::free_list pages
//  * first fit
//
// 10. local::free_list allocation strategy
//  * allow externally to register a callback to control allocation strategy
//    * hint of how much memory will be needed generally or specific bucketSz
//    * hint to over allocate by having a % in local free list
//    * hint that current allocation usage is baseline and should be kept
//      at least current available memory
//
// 11. memory leaks detect
//  * print statistics for all non-freed memory
//
// 12. assert less than sbrk(0)
// * sp_free(ptr < sbrk(0)), sp_realloc(ptr < sbrk(0)) ...
//
// 13. API
// * sp::bucket_size sp_round(std::size_t length);

// TODO optimizations
// - Some kind of TL cache with a reference to the most referenced Pools used
// for non-TL free:ing.
// - A range of the highest & lowest memory address used to determine if it is
// necessary to walk through the Nodes
//    - on PoolsRAII level
//    - on Pool level
// - An optimized collections of Pool:s used for free:ing in addition to the
//   Pool[60] used for allocating. balanced tree for Pool which are non-empty.
// - skip TL Pool when iterating global::free() since we have already handled
// it.

// TODO
// execute mprotect system call on unmapped memory present in global & local
// pool,write protect is maybe enough? only used when we are in debug mode!

// TODO
// - Look over where we use atomic<>
//  * if atomic is only used for cas() should it be release&acquire or seq_cst?

// TODO feature
// - alloc_aligned() - Find a bucket which have to required alignment
// - free(ptr,size) - support free with size hint
// - implement c++ new() interface
// - implement malloc interface
// - LD_PRELUDE
// - benchmark
//
// TODO change to mmap with one big area so we sp_malloc can coexist with another
// malloc without interfering with it by changing brk()

// thread local Pools {{{
static thread_local local::Pools local_pools;
// }}}

// global memory Arena {{{
static global::State global_state;
// }}}

/*
 *===========================================================
 *=======DEBUG===============================================
 *===========================================================
 */
#ifdef SP_TEST
namespace debug {
std::size_t
malloc_count_alloc() {
  if (local_pools.pools) {
    return alloc_count_alloc(*local_pools.pools);
  }
  return 0;
}

std::size_t
malloc_count_alloc(std::size_t sz) {
  if (local_pools.pools) {
    return alloc_count_alloc(*local_pools.pools, sz);
  }
  return 0;
}

void
force_reclaim_orphan_tl() {
  stuff_force_reclaim_orphan(global_state);
} // debug::force_reclaim_orphan_tl()

std::vector<std::tuple<void *, std::size_t>>
global_get_free() {
  return global_get_free(global_state);
} // debug::global_get_free()

} // namespace debug
#endif
/*
 *===========================================================
 *=======PUBLIC==============================================
 *===========================================================
 */
void *
sp_malloc(std::size_t length) noexcept {
  if (length == 0) {
    return nullptr;
  }

  auto &lpools = local_pools;
  lpools.init(global_state);

  return shared::alloc(global_state, lpools, length);
} // ::sp_malloc()

bool
sp_free(void *const ptr) noexcept {
  if (!ptr) {
    return true;
  }

  using shared::FreeCode;
  auto result = FreeCode::NOT_FOUND;

  auto &lpools = local_pools;
  if (lpools.pools) {
    shared::State state(global_state, local_pools, local_pools);
    result = shared::free(state, ptr);
    assert(result != FreeCode::FREED_RECLAIM);
  }

  if (result == FreeCode::NOT_FOUND) {
    result = global::free(global_state, lpools, ptr);
  }

  return result == FreeCode::FREED || result == FreeCode::FREED_RECLAIM;
} // ::sp_free()

std::size_t
sp_usable_size(void *const ptr) noexcept {
  if (!ptr) {
    return 0;
  }

  auto &lpools = local_pools;
  if (lpools.pools) {
    auto result = shared::usable_size(lpools, ptr);
    if (result) {
      return std::size_t(result.get());
    }
  }

  auto result = global::usable_size(ptr);
  return result.get_or(std::size_t(0));
} // ::sp_usable_size()

void *
sp_realloc(void *const ptr, std::size_t length) noexcept {
  if (!ptr) {
    return nullptr;
  }

  if (length == 0) {
    sp_free(ptr);
    return nullptr;
  }

  auto nop = shared::FreeCode::NOT_FOUND;
  // TODO only required init() when length < bucket->size
  local_pools.init(global_state);
  shared::State state(global_state, local_pools, local_pools);
  auto lresult = shared::realloc(state, ptr, length, nop);
  if (lresult) {
    return lresult.get();
  }
  assert(nop == shared::FreeCode::NOT_FOUND);

  auto result = global::realloc(global_state, local_pools, ptr, length);
  void *const def = nullptr;
  return result.get_or(def);
} //::sp_realloc
