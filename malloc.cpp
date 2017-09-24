#include <cassert>
#include <mutex>
#include <tuple>

#include "ReadWriteLock.h"
#include "bitset/Bitset.h"
#include <algorithm>
#include <utility>

#include <array>
#include <atomic>

#include "global.h"
#include "malloc.h"
#include "shared.h"

// http://preshing.com/20131125/acquire-and-release-fences-dont-work-the-way-youd-expect/

// # global
// TODO how to coaless global::free_list pages?
// # local
// TODO add local::free_list
// TODO coaless local::free_list when allocating thread looks for matching pages

// {{{
static thread_local local::Pools internal_pools;
// }}}

namespace local {

static std::size_t index_of(std::size_t size) noexcept {
  return (size / 8) - 1;
} // local::index_of()

static local::Pool &pool_for(local::Pools &pools, std::size_t sz) noexcept {
  std::size_t index = index_of(sz);
  return pools[index];
} // local::pool_for()

static void *alloc(std::size_t sz) noexcept {
  // printf("local::alloc(%zu)\n", sz);
  // TODO local alloc
  return global::alloc(sz);
} // local::alloc()

/*
 * @start - node start
 * desc   -
 */
static header::Node *next_node(header::Node *const start) noexcept {
  auto header = header::node(start);
  return header->next.load();
} // local::next_node()

/*
 * @param start - extent start
 * @param index -
 * @desc  -
 */
static void *pointer_at(header::Node *start, std::size_t index) noexcept {
  // printf("pointer_at(start,index(%zu))\n", index);
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
    assert(next->type == header::NodeType::INTERMEDIATE);

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
static header::Node *next_extent(header::Node *start) noexcept {
  assert(start != nullptr);
  assert(start->type == header::NodeType::HEAD);

  size_t hdrSz(header::SIZE);
  size_t buckets = start->buckets;
node_start:
  const size_t dataSz(start->node_size - hdrSz);

  assert(start->bucket_size > 0);
  size_t nodeBuckets = std::min(dataSz / start->bucket_size, buckets);
  buckets = buckets - nodeBuckets;

  header::Node *const next = next_node(start);

  if (buckets > 0) {
    hdrSz = sizeof(header::Node);

    assert(next != nullptr);
    assert(next->type == header::NodeType::INTERMEDIATE);

    start = next;

    goto node_start;
  } else {
    if (next) {
      if (next->type == header::NodeType::INTERMEDIATE) {
        // We get here because a concurrent expand of this extent is going on by
        // the allocating thread.

        // TODO will this work?
        goto node_start;
      }
    }
    return next;
  }
} // local::next_intent()

/*
 * @start - extent start
 * desc  - Used by malloc
 */
static void *reserve(header::Node *const node) noexcept {
  header::Extent *const eHdr = header::extent(node);

  auto &reservations = eHdr->reserved;
  // printf("reservations.swap_first(true,buckets(%zu))\n", nHdr->buckets);
  const std::size_t index = reservations.swap_first(true, node->buckets);
  if (index != reservations.npos) {
    return pointer_at(node, index);
  }
  return nullptr;
} // local::reserve()

/*
 * @dealloc - the same pointer as received from malloc
 * desc     - Used by free
 */
static bool free(void *const ptr) noexcept {
  // TODO
  // header::Extent *ext = extent_for(ptr);
  // if (ext) {
  //   const std::size_t idx = 0;
  //   if (!eHdr->reserved.set(idx, false)) {
  //     // double free is a runtime fault
  //     assert(false); // TODO is runtime failure
  //   }
  //   // TODO reclaim node?
  //   return true;
  // }
  return false;
} // local::free()

static std::size_t calc_min_node(std::size_t bucketSz) noexcept {
  assert(bucketSz >= 8);
  assert(bucketSz % 8 == 0);

  constexpr std::size_t min_alloc = SP_MALLOC_PAGE_SIZE;
  constexpr std::size_t max_alloc = min_alloc * 4;
  if (bucketSz + header::SIZE > max_alloc) {
    return util::round_up(bucketSz + header::SIZE, min_alloc);
  }

  constexpr std::size_t lookup[] = //
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
  return lookup[util::trailing_zeros(bucketSz >> 3)];
}

/*
 * @bucketSz - Normailzed size of bucket
 * desc  - Used by malloc
 */
static header::Node *alloc_extent(std::size_t bucketSz) noexcept {
  // printf("alloc_extent(%zu)\n", bucketSz);
  std::size_t nodeSz = calc_min_node(bucketSz);
  void *const raw = alloc(nodeSz);
  if (raw) {
    return header::init_node(raw, nodeSz, bucketSz);
  }
  return nullptr;
} // local::alloc_extent()

/*
 * @size - extent start
 * desc  - Used by malloc
 */
static void extend_extent(void *const start) noexcept {
  // TODO
} // local::extend_extent()

static bool in_node_range(const header::Node *const node,
                          void *const ptr) noexcept {
  assert(node != nullptr);
  assert(ptr != nullptr);

  uintptr_t start = reinterpret_cast<uintptr_t>(node);
  uintptr_t end = start + node->node_size;

  uintptr_t compare = reinterpret_cast<uintptr_t>(ptr);
  return compare >= start && compare < end;
}

static header::Node *node_for(Pool &pool, void *const ptr) noexcept {
  sp::SharedLock guard(pool.lock);
  if (guard) {
    header::Node *current = pool.start.load(std::memory_order_acquire);
  start:
    if (current) {
      if (in_node_range(current, ptr)) {
        return current;
      }
      current = current->next.load(std::memory_order_acquire);
      goto start;
    }
  }
  return nullptr;
}

static header::Node *node_for(Pools &pools, void *const ptr) noexcept {
  const std::uintptr_t raw = reinterpret_cast<std::uintptr_t>(ptr);
  const std::size_t tz = util::trailing_zeros(raw);

  const std::size_t offset = 1 << tz;
  if (offset < 8) {
    // should be a runtime fault, the minimum alignment is 8
    assert(false);
    return nullptr;
  }
  // std::size_t max = offset >= sizeof(header::Node) ? Pools::BUCKETS : tz + 1;
  const std::size_t max = Pools::BUCKETS;
  for (std::size_t i(0); i < max; ++i) {
    header::Node *result = node_for(pools[i], ptr);
    if (result) {
      return result;
    }
  }
  return nullptr;
} // local::node_for()

} // namespace local

/*
 *===========================================================
 *=======PUBLIC==============================================
 *===========================================================
 */
static header::Node *enqueue_new_extent(std::atomic<header::Node *> &w,
                                        std::size_t bucketSz) noexcept {
  header::Node *const current = local::alloc_extent(bucketSz);
  if (current) {
    // TODO some kind of fence to ensure construction before publication
    // std::atomic_thread_fence(std::memory_order_release);
    header::Node *start = nullptr;
    if (!w.compare_exchange_strong(start, current)) {
      // TODO local::dealloc_extent(current)
      // should never fail
      assert(false);
    }
  }
  return current;
}

void *sp_malloc(std::size_t length) noexcept {
  const std::size_t bucketSz = util::round_even(length);
  local::Pool &pool = pool_for(internal_pools, bucketSz);

  header::Node *start = pool.start.load(std::memory_order_acquire);
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
        // TODO support expand extent
        start = enqueue_new_extent(start->next, bucketSz);
        if (start) {
          goto reserve_start;
        }
        // out of memory
        return nullptr;
      }
    }
  } else {
    // only TL allowed to malloc meaning no alloc contention
    header::Node *const current = enqueue_new_extent(pool.start, bucketSz);
    if (current) {
      void *const result = local::reserve(current);
      // since only one allocator this must succeed
      assert(result != nullptr);
      return result;
    } else {
      // out of memory
      return nullptr;
    }
  }
  // should never get to here
  assert(false);
} // sp_malloc()

void sp_free(void *const dealloc) noexcept {
  // TODO a initial std::thread_atomic_fence_acquire(); required
  // the allocating thread is required to atomic_release it before the free
  // thread can see it and there fore only the first initial acquire fence.
  // Because any acquire fence since it can be another thread that performs the
  // free than the one allocating it
  // -----
  // maybe have a tag in the node header like Thread id to make global::free
  // easier.
  // but we need a way to arbitrary find the node header from a bucketIdx
  if (!local::free(dealloc)) {
    // not the same free:ing as malloc:ing thread
    if (!global::free(dealloc)) {
      assert(false);
    }
  }
} // sp_free()

std::size_t sp_sizeof(void *const p) noexcept {
  const header::Node *const node = local::node_for(internal_pools, p);
  if (node) {
    return node->bucket_size;
  }
  return 0;
}
