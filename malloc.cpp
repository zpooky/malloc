#include <algorithm>
#include <cassert>
#include <cstring>
#include <mutex>
#include <tuple>
#include <utility>

#include "ReadWriteLock.h"
#include "bitset/Bitset.h"
#include "malloc_debug.h"

#include <array>
#include <atomic>

#include "global.h"
#include "malloc.h"
#include "shared.h"
#include "stuff.h"

// http://preshing.com/20131125/acquire-and-release-fences-dont-work-the-way-youd-expect/

// # version 2.0
// - from a pointer get the node header and in the node header get a pointer to
// the extent header to support random access free(). Maybe have a tag in the
// node header like Thread id to make global::free easier.

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
// 5. global::free_list sbrk (shrinking/reclamation/release/dealloc/free)
//
// 9. global::free_list
//  * how to better coalesce global::free_list pages
//  * first fit

// {{{
static thread_local local::Pools internal_pools;
// }}}

namespace local {

static std::size_t
pool_index(std::size_t size) noexcept {
  return util::trailing_zeros(size >> 3);
} // local::pool_index()

static std::size_t
node_data_size(header::Node *node) noexcept {
  std::size_t result = node->node_size;
  if (node->type == header::NodeType::HEAD) {
    assert(result >= header::SIZE);
    result -= header::SIZE;
  } else {
    assert(result >= sizeof(header::Node));
    result -= sizeof(header::Node);
  }
  return result;
}

static uintptr_t
node_data_start(header::Node *node) noexcept {
  uintptr_t result = reinterpret_cast<uintptr_t>(node);
  result += sizeof(header::Node);
  if (node->type == header::NodeType::HEAD) {
    result += sizeof(header::Extent);
  }
  return result;
}

static local::Pool &
pool_for(local::Pools &pools, std::size_t sz) noexcept {
  std::size_t index = pool_index(sz);
  return pools[index];
} // local::pool_for()

static void *
alloc(std::size_t sz) noexcept {
  // printf("local::alloc(%zu)\n", sz);
  // TODO local alloc
  return global::alloc(sz);
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
static header::Node *
next_extent(header::Node *start) noexcept {
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
static void *
reserve(header::Node *const node) noexcept {
  header::Extent *const eHdr = header::extent(node);

  auto &reservations = eHdr->reserved;
  // printf("reservations.swap_first(true,buckets(%zu))\n", nHdr->buckets);
  const std::size_t index = reservations.swap_first(true, node->buckets);
  if (index != reservations.npos) {
    return pointer_at(node, index);
  }
  return nullptr;
} // local::reserve()

static std::size_t
calc_min_node(std::size_t bucketSz) noexcept {
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
  return lookup[pool_index(bucketSz)];
}

/*
 * @bucketSz - Normailzed size of bucket
 * desc  - Used by malloc
 */
static header::Node *
alloc_extent(std::size_t bucketSz) noexcept {
  // printf("alloc_extent(%zu)\n", bucketSz);
  std::size_t nodeSz = calc_min_node(bucketSz);
  void *const raw = alloc(nodeSz);
  if (raw) {
    return header::init_node(raw, nodeSz, bucketSz);
  }
  return nullptr;
} // local::alloc_extent()

static bool
node_in_range(const header::Node *const node, void *const ptr) noexcept {
  assert(node != nullptr);
  assert(ptr != nullptr);

  uintptr_t start = reinterpret_cast<uintptr_t>(node);
  uintptr_t end = start + node->node_size;

  uintptr_t compare = reinterpret_cast<uintptr_t>(ptr);
  return compare >= start && compare < end;
} // local::in_node_range()

template <typename Res, typename Arg>
using NodeFor = Res (*)(header::Node *, Arg &);

template <typename Res, typename Arg>
static util::maybe<Res>
node_for(Pool &pool, void *const search, NodeFor<Res, Arg> f,
         Arg &arg) noexcept {
  sp::SharedLock guard(pool.lock);
  if (guard) {
    header::Node *current = pool.start.load(std::memory_order_acquire);
  start:
    if (current) {
      if (node_in_range(current, search)) {
        return util::maybe<Res>(f(current, arg));
      }
      current = current->next.load(std::memory_order_acquire);
      goto start;
    }
  }
  return {};
} // local::node_for()

static int
node_index_of(header::Node *node, void *ptr) noexcept {
  const std::size_t data_size = node_data_size(node);
  const uintptr_t data_start = node_data_start(node);
  const uintptr_t data_end = data_start + data_size;
  const uintptr_t search = reinterpret_cast<uintptr_t>(ptr);

  if (search >= data_start && search < data_end) {
    uintptr_t it = data_start;
    std::size_t index = 0;

    // TODO make better
    while (it < data_end) {
      if (it == search) {
        return index;
      }
      ++index;
      it += node->bucket_size;
    }
    assert(false);
  }
  return -1;
}

static std::size_t
node_indecies_in(header::Node *node) noexcept {
  std::size_t result = node_data_size(node);
  return std::size_t(result / node->bucket_size);
}

template <typename Res, typename Arg>
using ExtFor = Res (*)(header::Node *, std::size_t, Arg &);

template <typename Res, typename Arg>
static util::maybe<Res>
extent_for(Pool &pool, void *const search, ExtFor<Res, Arg> f,
           Arg &arg) noexcept {
  sp::SharedLock guard(pool.lock);
  if (guard) {
    header::Node *current = pool.start.load(std::memory_order_acquire);
    header::Node *extent = nullptr;
    std::size_t index{0};
  start:
    if (current) {
      if (current->type == header::NodeType::HEAD) {
        extent = current;
        index = 0;
      }

      int nodeIdx = node_index_of(current, search);
      if (nodeIdx != -1) {
        assert(extent != nullptr);
        index += nodeIdx;

        return util::maybe<Res>(f(extent, index, arg));
      }
      index += node_indecies_in(current);

      current = current->next.load(std::memory_order_acquire);
      goto start;
    }
  }

  return {};
} // local::extent_for()

template <typename Res, typename Arg>
using PoolsFind = util::maybe<Res> (*)(Pool &, void *, Arg &);

template <typename Res, typename Arg>
static util::maybe<Res>
pools_find(Pools &pools, void *const search, PoolsFind<Res, Arg> f,
           Arg &arg) noexcept {
  std::uintptr_t rawSearch = reinterpret_cast<std::uintptr_t>(search);
  std::size_t trail0 = util::trailing_zeros(rawSearch);

  const std::size_t offset = 1 << trail0;
  if (offset < 8) {
    // should be a runtime fault, the minimum alignment is 8
    assert(false);
    return {};
  }
  // TODO
  // std::size_t max = offset >= sizeof(header::Node) ? Pools::BUCKETS : tz + 1;
  const std::size_t max = Pools::BUCKETS;
  for (std::size_t i(0); i < max; ++i) {
    auto result = f(pools[i], search, arg);
    if (result) {
      return result;
    }
  }

  return {};
} // local::pools_find()

static bool
should_expand_extent(header::Node *) {
  return false;
}

static header::Node *
enqueue_new_extent(std::atomic<header::Node *> &w,
                   std::size_t bucketSz) noexcept {
  header::Node *const current = local::alloc_extent(bucketSz);
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

static bool
perform_free(header::Extent *ext, std::size_t idx) noexcept {
  if (!ext->reserved.set(idx, false)) {
    // double free is a runtime fault
    assert(false);
  }
  // TODO reclaim node?
  return true;
}

/*
 * @dealloc - the same pointer as received from malloc
 * desc     - Used by free
 */
static bool
free(Pools &pools, void *const ptr) noexcept {
  auto arg = nullptr;
  auto res = pools_find<bool, std::nullptr_t>(
      pools, ptr, //
      [](local::Pool &p, void *search, std::nullptr_t &a) -> util::maybe<bool> {
        return extent_for<bool, std::nullptr_t>(
            p, search, //
            [](header::Node *head, std::size_t idx, std::nullptr_t &) -> bool {
              header::Extent *ext = header::extent(head);
              return perform_free(ext, idx);
            },
            a);
      },
      arg);

  bool def = false;
  return res.get_or(def);
} // local::free()

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
  for (std::size_t it = 8; it > 0; it <<= 1) {
    result += malloc_count_alloc(it);
  }
  return result;
}

static std::size_t
count_reserved(header::Extent &ext) {
  std::size_t result(0);
  auto &bitset = ext.reserved;
  std::size_t idx(0);
  for (; idx < bitset.size(); ++idx) {
    if (bitset.test(idx)) {
      result++;
    }
  }

  return result;
}

std::size_t
malloc_count_alloc(std::size_t sz) {
  std::size_t result(0);
  local::Pool &pool = local::pool_for(internal_pools, sz);
  sp::SharedLock guard(pool.lock);
  if (guard) {
    auto current = pool.start.load();
  start:
    if (current) {
      auto nodeHdr = header::node(current);
      auto extentHdr = header::extent(nodeHdr);
      result += count_reserved(*extentHdr);
      current = local::next_extent(nodeHdr);
      goto start;
    }
  }
  return result;
}
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

  const std::size_t bucketSz = util::round_even(length);
  local::Pool &pool = pool_for(internal_pools, bucketSz);

  sp::SharedLock guard{pool.lock};
  if (guard) {

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

          if (local::should_expand_extent(start)) {
            local::expand_extent(start);
          } else {
            start = local::enqueue_new_extent(start->next, bucketSz);
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
      header::Node *current = local::enqueue_new_extent(pool.start, bucketSz);
      if (current) {
        void *const result = local::reserve(current);
        // since only one allocator this must succeed
        assert(result != nullptr);
        return result;
      }

      // out of memory
      return nullptr;
    }
  } // SharedLock

  // should never get to here
  assert(false);
  return nullptr;
} // ::sp_malloc()

bool
sp_free(void *const ptr) noexcept {
  if (!ptr) {
    return true;
  }

  if (!local::free(internal_pools, ptr)) {
    // not the same free:ing as malloc:ing thread
    if (!stuff::free(ptr)) {
      // unknown address
      assert(false);
      return false;
    }
  }
  return true;
} // ::sp_free()

std::size_t
sp_sizeof(void *const ptr) noexcept {
  // TODO support global sizeof
  if (ptr) {
    std::nullptr_t arg = nullptr;
    auto &pools = internal_pools;

    auto res = local::pools_find<std::size_t, std::nullptr_t>(
        pools, ptr, //
        [](local::Pool &pool, void *search,
           std::nullptr_t &a) -> util::maybe<std::size_t> { //
          return local::node_for<std::size_t, std::nullptr_t>(
              pool, search, //
              [](header::Node *current, std::nullptr_t &) -> std::size_t {
                //
                return current->bucket_size;
              },
              a);
        },
        arg);
    if (res) {
      return res.get();
    }
  }
  return 0;
} // ::sp_sizeof()

void *
sp_realloc(void *ptr, std::size_t length) noexcept {
  // TODO support global realloc
  if (length == 0) {
    sp_free(ptr);
    return nullptr;
  }

  using Arg = std::tuple<void *, std::size_t>;
  Arg arg(ptr, length);

  auto result = local::pools_find<void *, Arg>(
      internal_pools, ptr, //
      [](local::Pool &pool, void *search, Arg &arg) {
        return local::extent_for<void *, Arg>(
            pool, search, //
            [](header::Node *head, std::size_t idx, Arg &arg) {

              std::size_t length = std::get<1>(arg);
              void *ptr = std::get<0>(arg);

              header::Extent *ext = header::extent(head);
              if (head->bucket_size < length) {

                void *nptr = sp_malloc(length); // TODO deadlock!
                if (nptr) {

                  memcpy(nptr, ptr, head->bucket_size);
                  // TODO move to somewhere else
                  local::perform_free(ext, idx);
                }

                return nptr;
              }

              return ptr;
            },
            arg);
      },
      arg);

  void *def = nullptr;
  return result.get_or(def);
}
