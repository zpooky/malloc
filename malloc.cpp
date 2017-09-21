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
static void *next_node(void *const start) noexcept {
  auto header = header::node(start);
  return header->next.load();
} // local::next_node()

/*
 * @param start - extent start
 * @param index -
 * @desc  -
 */
static void *pointer_at(void *start, std::size_t index) noexcept {
  // printf("pointer_at(start,index(%zu))\n", index);
  // Pool[Extent[Node[nodeHDR,extHDR],Node[nodeHDR]...]...]
  // The first NodeHeader in the extent contains data while intermediate
  // NodeHeader does not containt this data.
  header::Node *nH = header::node(start);
  size_t hdrSz(header::SIZE);
  size_t buckets = nH->buckets;
  const size_t bucket_size = nH->bucket_size;
node_start:
  const size_t node_size = nH->node_size;
  const size_t data_size(node_size - hdrSz);

  size_t nodeBuckets = std::min(data_size / nH->bucket_size, buckets);
  if (index < nodeBuckets) {
    // the index is in range of current node
    uintptr_t startPtr = reinterpret_cast<uintptr_t>(start);
    uintptr_t data_start = startPtr + hdrSz;

    return reinterpret_cast<void *>(data_start + (index * bucket_size));
  }
  // the index is out of range, go to next node

  void *const next = next_node(start);
  index = index - nodeBuckets;
  buckets = buckets - nodeBuckets;

  if (buckets > 0) {
    assert(next != nullptr);
    header::Node *iNHdr = header::node(next);
    assert(iNHdr->type == header::NodeType::INTERMEDIATE);

    start = next;
    // the same extent but a new node
    hdrSz = sizeof(header::Node);
    goto node_start;
  } else {
    // out of bound
    assert(false);
  }
} // local::pointer_at()

/*
 * @start - extent start
 * desc  - Used by malloc & free
 */
static void *next_extent(void *start) noexcept {
  assert(start != nullptr);
  header::Node *const nH = header::node(start);
  assert(nH != nullptr);
  assert(nH->bucket_size > 0);

  size_t hdrSz(header::SIZE);
  size_t buckets = nH->buckets;
node_start:
  const size_t dataSz(nH->node_size - hdrSz);

  size_t nodeBuckets = std::min(dataSz / nH->bucket_size, buckets);
  buckets = buckets - nodeBuckets;

  void *const next = next_node(start);

  if (buckets > 0) {
    hdrSz = sizeof(header::Node);

    assert(next != nullptr);
    header::Node *iNHdr = header::node(next);
    assert(iNHdr->type == header::NodeType::INTERMEDIATE);

    start = next;

    goto node_start;
  } else {
    if (next) {
      header::Node *iNHdr = header::node(next);
      if (iNHdr->type == header::NodeType::INTERMEDIATE) {
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
static void *reserve(void *const start) noexcept {
  header::Node *const nHdr = header::node(start);
  header::Extent *const eHdr = header::extent(start);

  auto &reservations = eHdr->reserved;
  // printf("reservations.swap_first(true,buckets(%zu))\n", nHdr->buckets);
  const std::size_t index = reservations.swap_first(true, nHdr->buckets);
  if (index != reservations.npos) {
    return pointer_at(start, index);
  }
  return nullptr;
} // local::reserve()

/*
 * @dealloc - the same pointer as received from malloc
 * desc     - Used by free
 */
static bool free(void *const dealloc) noexcept {
  // TODO
  uintptr_t ptr = reinterpret_cast<uintptr_t>(dealloc);
  std::size_t maxIdx = (ptr & header::SIZE) == 0 ? 63 : 0;

  // TODO fence where apporopriate
  std::atomic_thread_fence(std::memory_order_acquire);

  header::Extent *eHdr = header::extent(nullptr); // TODO
  const std::size_t idx = 0;
  if (true) {
    // a thread local alloc&free
    if (!eHdr->reserved.set(idx, false)) {
      // dubble free
      assert(false);
    }
    // TODO reclaim node?
  } else {
    // not the same free:ing as malloc:ing thread
    return global::free(dealloc);
  }
  return true;
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
static void *alloc_extent(std::size_t bucketSz) noexcept {
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

} // namespace local

// {{{
static thread_local local::Pools pools;
// }}}

/*
 *===========================================================
 *=======PUBLIC==============================================
 *===========================================================
 */
void *sp_malloc(std::size_t length) noexcept {
  const std::size_t bucketSz = util::round_even(length);
  local::Pool &pool = pool_for(pools, bucketSz);

  void *start = pool.start.load(std::memory_order_acquire);
  if (start) {
  reserve_start:
    void *result = local::reserve(start);
    if (result) {
      return result;
    } else {
      void *const next = local::next_extent(start);
      if (next) {
        start = next;
        goto reserve_start;
      } else {
        assert(false);
        // TODO  either extend extent or allocate new extent
        // does not need to do the local::alloc under the exclusive lock
        // since we will only have one TL allocator at a time
      }
    }
  } else {
    // only TL allowed to malloc meaning no alloc contention
    void *const extent = local::alloc_extent(bucketSz);
    if (extent) {
      void *const result = local::reserve(extent);
      // TODO some kind of fence to ensure construction before publication
      // std::atomic_thread_fence(std::memory_order_release);
      if (pool.start.compare_exchange_strong(start, extent)) {
        return result;
      } else {
        // should never fail
        assert(false);
      }
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
  if (!local::free(dealloc)) {
    assert(false);
  }
} // sp_free()
