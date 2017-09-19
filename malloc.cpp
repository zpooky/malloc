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
// TODO free_list header with sp::ReadWriteLock instead global. node locks the
// next so we have to hold the lock of the node pointing to current node do
// anything
namespace local {

static constexpr std::size_t HEADER_SIZE(sizeof(header::Node) +
                                         sizeof(header::Extent));
static std::size_t index_of(std::size_t size) noexcept {
  return (size / 8) - 1;
}

static local::Pool &pool_for(local::Pools &pools, std::size_t sz) noexcept {
  std::size_t index = index_of(sz);
  return pools[index];
} // pool_for()

static std::tuple<void *, std::size_t> alloc(std::size_t sz) noexcept {
  // TODO
  auto ret = global::alloc(sz);
  if (ret) {
    return std::make_tuple(ret, sz);
  }
  return std::make_tuple(nullptr, 0);
} // alloc()

/*
 * @start - node start
 * desc   -
 */
static void *next_node(void *const start) noexcept {
  auto header = header::node(start);
  return header->next.load();
} // next_node()

/*
 * @param start - extent start
 * @param index -
 * @desc  -
 */
static void *pointer_at(void *start, std::size_t index) noexcept {
  // Pool[Extent[Node[nodeHDR,extHDR],Node[nodeHDR]...]...]
  // The first NodeHeader in the extent contains data while intermediate
  // NodeHeader does not containt this data.
  header::Node *nH = header::node(start);
  size_t hdrSz(HEADER_SIZE);
  size_t buckets = nH->size;
node_start:
  const size_t dataSz(nH->rawNodeSize - hdrSz);

  size_t nodeBuckets = std::min(dataSz / nH->bucket, buckets);
  if (index < nodeBuckets) {
    // the index is in range of current node
    uintptr_t startPtr = reinterpret_cast<uintptr_t>(start);
    uintptr_t data_start = startPtr + hdrSz;

    return reinterpret_cast<void *>(data_start + (index * nH->bucket));
  } else {
    // the index is out of range go to next node or extent

    void *next = next_node(start);
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
      return nullptr;
    }
  }
} // pointer_at()

/*
 * @start - extent start
 * desc  - Used by malloc & free
 */
void *next_extent(void *start) noexcept {
  assert(start != nullptr);
  header::Node *nH = header::node(start);
  assert(nH != nullptr);

  size_t hdrSz(HEADER_SIZE);
  size_t buckets = nH->size;
node_start:
  const size_t dataSz(nH->rawNodeSize - hdrSz);

  size_t nodeBuckets = std::min(dataSz / nH->bucket, buckets);
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
} // next_intent()

/*
 * @start - extent start
 * desc  - Used by malloc
 */
void *reserve(void *const start) noexcept {
  header::Node *nHdr = header::node(start);
  header::Extent *eHdr = header::extent(start);

  const size_t index = eHdr->reserved.swap_first(true, nHdr->size);
  if (index != eHdr->reserved.npos) {
    return pointer_at(start, index);
  }
  return nullptr;
} // reserve()

/*
 * @dealloc - the same pointer as received for mammloc
 * desc  - Used by free
 */
bool free(void *const dealloc) noexcept {
  // TODO
  uintptr_t ptr = reinterpret_cast<uintptr_t>(dealloc);
  std::size_t maxIdx = (ptr & HEADER_SIZE) == 0 ? 63 : 0;

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
} // free()

/*
 * @bucketSz - Normailzed size of bucket
 * desc  - Used by malloc
 */
void *alloc_extent(std::size_t bucketSz) noexcept {
  // TODO calculate size of extent
  std::size_t extentSz(0);
  auto mem = alloc(extentSz);
  void *const start = std::get<0>(mem);
  std::size_t length = std::get<1>(mem);
  if (start) {
    return header::init_extent(start, bucketSz, length);
  }
  return nullptr;
}

/*
 * @size - extent start
 * desc  - Used by malloc
 */
void extend_extent(void *const start) noexcept {
  // TODO
}

} // namespace local

// {{{
static thread_local local::Pools pools;
// }}}

/*
 *===========================================================
 *=======PUBLIC==============================================
 *===========================================================
 */
void *sp_malloc(std::size_t sz) noexcept {
  const std::size_t bucketSz = util::round_even(sz);
  local::Pool &pool = pool_for(pools, bucketSz);

  void *start = pool.start.load();
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
        // TODO  either extend extent or allocate new extent
        // does not need to do the local::alloc under the exclusive lock
        // since we will only have one TL allocater at a time
      }
    }
  } else {
    // TODO shared lock not required here

    // only TL allowed to malloc meaning no alloc contention
    void *const extent = local::alloc_extent(bucketSz);
    if (extent) {
      void *const result = local::reserve(extent);
      std::atomic_thread_fence(std::memory_order_release);
      if (pool.start.compare_exchange_strong(start, extent)) {
        return result;
      } else {
        // somehow we failed to cas
        assert(false);
      }
    } else {
      // out of memory
      return nullptr;
    }
  }
  return nullptr;
} // sp_malloc()

void sp_free(void *const dealloc) noexcept {
  // TODO a initial std::thread_atomic_fence_acquire(); required
  // the allocating thread is required to atomuc_release it before the free
  // thread can see it and there fore only the first initial aquire fence.
  // Because any _aquire fence since it can be another thread that performs the
  // free than the one allocating it
  if (!local::free(dealloc)) {
    assert(false);
  }
} // sp_free()
