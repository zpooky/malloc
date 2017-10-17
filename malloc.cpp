#include <algorithm>
#include <cassert>
#include <mutex>
#include <tuple>
#include <utility>

#include "ReadWriteLock.h"
#include "bitset/Bitset.h"
#include "malloc_debug.h"

#include <array>
#include <atomic>

#include "free.h"
#include "global.h"
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
// 4. Differentiate global and global_Pool storage
//  * refactor to two different files
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
//    * hint to over allocate by haveing a % in local free list
//
// 11. memory leaks detect
//  * print statistics for all non-freed memory
//
// 12. assert less than sbrk(0)
// * sp_free(ptr < sbrk(0)), sp_realloc(ptr < sbrk(0)) ...

// TODO optimizations
// - Some kind of TL cache with a reference to the most referenced Pools used
// for non-TL free:ing.
// - A range of the highest & lowest memory address used to determine if it is
// necessary to walk through the Nodes
//    - on PoolsRAII level
//    - on Pool level
// - An optimized collections of Pool:s used for free:ing in addition to the
//   Pool[60] used for allocating
// - skip `this` when global::free()

// {{{
static thread_local local::Pools local_pools;
// }}}

namespace local {

static std::size_t
pool_index(sp::bucket_size sz) noexcept {
  assert(sz % 8 == 0);
  return util::trailing_zeros(sz) - std::size_t(3);
} // local::pool_index()

static local::Pool &
pool_for(local::Pools &pools, std::bucket_size sz) noexcept {
  const std::size_t index = pool_index(sz);
  return pools[index];
} // local::pool_for()

static void *
alloc(local::Pools &pools, sp::node_size sz) noexcept {
  return nullptr;
} // local::alloc()

/*
 * @start - node start
 * desc   -
 */
static header::Node *
next_node(header::Node *const start) noexcept {
  return start->next.load();
} // local::next_node()

/*
 * @param start - extent start
 * @param index -
 * @desc  -
 */
static void *
pointer_at(header::Node *start, std::size_t index) noexcept {
  // Pool[Extent[Node[nodeHDR,extHDR],Node[nodeHDR]...]...]
  // The first NodeHeader in the extent contains data while intermediate
  // NodeHeader does not containt this data.
  assert(start->type == header::NodeType::HEAD);
  size_t hdrSz(header::SIZE);
  size_t buckets = start->buckets;
  const size_t bucket_size = start->bucket_size;
node_start:
  const size_t node_size = start->node_size;
  const size_t data_size(node_size - hdrSz);

  size_t nodeBuckets = std::min(data_size / start->bucket_size, buckets);
  if (index < nodeBuckets) {
    // the index is in range of current node
    uintptr_t startPtr = reinterpret_cast<uintptr_t>(start);
    uintptr_t data_start = startPtr + hdrSz;

    return reinterpret_cast<void *>(data_start + (index * bucket_size));
  }
  // the index is out of range, go to next node

  header::Node *const next = next_node(start);
  index = index - nodeBuckets;
  buckets = buckets - nodeBuckets;

  if (buckets > 0) {
    assert(next != nullptr);
    assert(next->type == header::NodeType::LINK);

    start = next;
    // the same extent but a new node
    hdrSz = sizeof(header::Node);
    goto node_start;
  } else {
    // out of bound
    // TODO implementation fault or runtime?
    assert(false);
  }
} // local::pointer_at()

/*
 * @start - extent start
 * desc  - Used by malloc & free
 */
static header::Node *
next_extent(header::Node *start) noexcept {
  assert(start != nullptr);
start:
  header::Node *const next = next_node(start);
  if (next) {
    if (next->type == header::NodeType::HEAD) {
      return next;
    }
    start = next;
    goto start;
  }
  return nullptr;
} // local::next_intent()

/*
 * @start - extent start
 * desc  - Used by malloc
 */
static void *
reserve(header::Node *const node) noexcept {
  if (node->type == header::NodeType::HEAD) {
    header::Extent *const eHdr = header::extent(node);

    auto &reservations = eHdr->reserved;
    // printf("reservations.swap_first(true,buckets(%zu))\n", nHdr->buckets);
    const std::size_t limit = node->buckets;
    const std::size_t index = reservations.swap_first(true, limit);
    if (index != reservations.npos) {
      return pointer_at(node, index);
    }
  }
  return nullptr;
} // local::reserve()

static sp::node_size
calc_min_node(sp::bucket_size bucketSz) noexcept {
  assert(bucketSz >= 8);
  assert(bucketSz % 8 == 0);

  constexpr sp::node_size min_alloc(SP_MALLOC_PAGE_SIZE);
  constexpr sp::node_size max_alloc(min_alloc * 4);

  constexpr sp::bucket_size atLeast(bucketSz + header::SIZE);
  if (atLeast > max_alloc) {
    return util::round_up(atLeast, min_alloc);
  }

  constexpr sp::node_size lookup[] = //
      {
          //
          /*___8:*/ min_alloc,
          /*__16:*/ min_alloc * 2,
          /*__32:*/ max_alloc,
          /*__64:*/ max_alloc,
          /*_128:*/ max_alloc,
          /*_256:*/ max_alloc,
          /*_512:*/ max_alloc,
          /*1024:*/ max_alloc,
          /*2048:*/ max_alloc,
          /*4096:*/ max_alloc,
          /*8192:*/ min_alloc * 5,
          //
      };
  return lookup[pool_index(bucketSz)];
}

/*
 * @bucketSz - Normailzed size of bucket
 * desc  - Used by malloc
 */
static header::Node *
alloc_extent(local::Pools &pools, sp::bucket_size bucketSz) noexcept {
  // printf("alloc_extent(%zu)\n", bucketSz);
  sp::node_size nodeSz = calc_min_node(bucketSz);
  void *const raw = alloc(pools, nodeSz);
  if (raw) {
    return header::init_node(raw, nodeSz, bucketSz);
  }
  return nullptr;
} // local::alloc_extent()

static bool
should_expand_extent(header::Node *) {
  return false;
}

static header::Node *
enqueue_new_extent(local::Pools &pools, std::atomic<header::Node *> &w,
                   sp::bucket_size bucketSz) noexcept {
  header::Node *const current = local::alloc_extent(pools, bucketSz);
  if (current) {
    // TODO some kind of fence to ensure construction before publication
    // std::atomic_thread_fence(std::memory_order_release);
    header::Node *start = nullptr;
    if (!w.compare_exchange_strong(start, current)) {
      // should never fail

      // TODO local::dealloc_extent(current)
      assert(false);
    }
  }
  return current;
} // local::enqueue_new_extent()

/*
 * @size - extent start
 * desc  - Used by malloc
 */
static void
expand_extent(void *const) noexcept {
  // TODO
} // local::extend_extent()

} // namespace local

/*
 *===========================================================
 *=======DEBUG===============================================
 *===========================================================
 */
#ifdef SP_TEST
namespace debug {
std::size_t
malloc_count_alloc() {
  std::size_t result(0);
  for (std::size_t i(8); i > 0; i <<= 1) {
    result += malloc_count_alloc(i);
  }
  return result;
}

static std::size_t
count_reserved(header::Extent &ext, std::size_t buckets) {
  std::size_t result(0);
  auto &b = ext.reserved;
  for (std::size_t idx(0); idx < buckets; ++idx) {
    if (b.test(idx)) {
      result++;
    }
  }

  return result;
}

std::size_t
malloc_count_alloc(std::size_t sz) {
  std::size_t result(0);
  local::Pool &pool = local::pool_for(local_pools, sz);
  sp::SharedLock guard(pool.lock);
  if (guard) {
    header::Node *current = pool.start.next.load();
  start:
    if (current) {
      assert(current->type == header::NodeType::HEAD);
      auto ext = header::extent(current);
      assert(ext != nullptr);
      result += count_reserved(*ext, current->buckets);

      current = local::next_extent(current);
      goto start;
    }
  }
  return result;
}
} // namespace debug
#endif
/*
 *===========================================================
 */
static void *
alloc(local::Pools &pools, sp::bucket_size sz) noexcept {
  sp::node_size nodeSz = void *result = local::alloc(pools, sz);
  if (!result) {
    result = global::alloc(sz);
    if (result) {
      pools.pools->total_alloc.fetch_add(sz);
    }
  }
  return result;
}
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

  const std::size_t bucketSz = util::round_even(length);
  local::Pool &pool = pool_for(local_pools, bucketSz);

  sp::SharedLock guard{pool.lock};
  if (guard) {

    header::Node *start = &pool.start;
    if (start) {

    reserve_start:
      void *result = local::reserve(start);
      if (result) {

        return result;
      } else {
        header::Node *const next = local::next_extent(start);
        if (next) {

          start = next;
          goto reserve_start;
        } else {

          if (local::should_expand_extent(start)) {
            local::expand_extent(start);
          } else {
            start =
                local::enqueue_new_extent(local_pools, start->next, bucketSz);
          }
          if (start) {
            goto reserve_start;
          }

          // out of memory
          return nullptr;
        }
      }
    } else {
      // only TL allowed to malloc meaning no alloc contention
      header::Node *current =
          local::enqueue_new_extent(local_pools, pool.start.next, bucketSz);
      if (current) {
        void *const result = local::reserve(current);
        // Since only one allocator this must succeed.
        assert(result);
        return result;
      }

      assert(false);
      // out of memory
      return nullptr;
    }
  } // SharedLock

  // We should never get to here
  assert(false);
  return nullptr;
} // ::sp_malloc()

bool
sp_free(void *const ptr) noexcept {
  if (!ptr) {
    return true;
  }

  using shared::FreeCode;

  auto result = shared::free(local_pools, ptr);
  if (result == FreeCode::NOT_FOUND) {
    result = global::free(ptr);
  }

  return result == FreeCode::FREED || result == FreeCode::FREED_RECLAIM;
} // ::sp_free()

std::size_t
sp_usable_size(void *const ptr) noexcept {
  if (!ptr) {
    return 0;
  }

  auto lresult = shared::usable_size(local_pools, ptr);
  if (lresult) {
    return lresult.get();
  }

  auto result = global::usable_size(ptr);
  return result.get_or(0);
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

  auto lresult = shared::realloc(local_pools, ptr, length);
  if (lresult) {
    return lresult.get();
  }

  auto result = global::realloc(ptr, length);
  return result.get_or(nullptr);
} //::sp_realloc
