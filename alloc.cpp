#include "alloc.h"

#include "ReadWriteLock.h"
#include "bitset/Bitset.h"
#include "global.h"

static void *
local_alloc(local::PoolsRAII &pools, sp::node_size sz) noexcept {
  // TODO
  return nullptr;
} // ::alloc()

static void *
alloc(local::PoolsRAII &pools, sp::node_size sz) noexcept {
  void *result = local_alloc(pools, sz);
  if (!result) {
    const std::size_t size(sz);
    result = global::alloc(size);
    if (result) {
      pools.total_alloc.fetch_add(size);
    }
  }
  return result;
} // ::alloc()

/*
 * @start - node start
 * desc   -
 */
static header::Node *
next_node(header::Node *const start) noexcept {
  return start->next.load();
} // ::next_node()

/*
 * @param start - extent start
 * @param index -
 * @desc  -
 */
static void *
pointer_at(header::Node *start, sp::index index) noexcept {
  // Pool[Extent[Node[nodeHDR,extHDR],Node[nodeHDR]...]...]
  // The first NodeHeader in the extent contains data while intermediate
  // NodeHeader does not containt this data.
  assert(start->type == header::NodeType::HEAD);
  size_t header_size(header::SIZE);
  sp::buckets buckets = start->buckets;
  const sp::bucket_size bucket_size = start->bucket_size;
node_start:
  const sp::node_size node_size = start->node_size;
  const sp::node_size data_area_size(node_size - header_size);

  sp::buckets nodeBuckets =
      std::min(data_area_size / start->bucket_size, buckets);
  if (index < nodeBuckets) {
    // the index is in range of current node
    uintptr_t startPtr = reinterpret_cast<uintptr_t>(start);
    uintptr_t data_start = startPtr + header_size;

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
    header_size = sizeof(header::Node);
    goto node_start;
  } else {
    // out of bound
    // TODO implementation fault or runtime?
    assert(false);
  }
} // ::pointer_at()

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
} // ::next_intent()

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
    const std::size_t limit(node->buckets);
    const sp::index index(reservations.swap_first(true, limit));
    if (index != reservations.npos) {
      return pointer_at(node, index);
    }
  }
  return nullptr;
} // ::reserve()

static sp::node_size
calc_min_node(sp::bucket_size bucketSz) noexcept {
  assert(bucketSz >= 8);
  assert(bucketSz % 8 == 0);

  constexpr sp::node_size min_alloc(SP_MALLOC_PAGE_SIZE);
  constexpr sp::node_size max_alloc(min_alloc * 4);

  const sp::bucket_size atLeast(bucketSz + header::SIZE);
  if (atLeast > max_alloc) {
    return sp::node_size{
        util::round_up(std::size_t(atLeast), std::size_t(min_alloc))};
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
  return lookup[shared::pool_index(bucketSz)];
}

/*
 * @bucketSz - Normailzed size of bucket
 * desc  - Used by malloc
 */
static header::Node *
alloc_extent(local::PoolsRAII &pools, sp::bucket_size bucketSz) noexcept {
  // printf("alloc_extent(%zu)\n", bucketSz);
  sp::node_size nodeSz = calc_min_node(bucketSz);
  void *const raw = alloc(pools, nodeSz);
  if (raw) {
    return header::init_extent(raw, nodeSz, bucketSz);
  }
  return nullptr;
} // ::alloc_extent()

static bool
should_expand_extent(header::Node *) {
  return false;
}

static header::Node *
enqueue_new_extent(local::PoolsRAII &pools, std::atomic<header::Node *> &w,
                   sp::bucket_size bucketSz) noexcept {
  header::Node *const current = ::alloc_extent(pools, bucketSz);
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
} // ::enqueue_new_extent()

/*
 * @size - extent start
 * desc  - Used by malloc
 */
static void
expand_extent(void *const) noexcept {
  // TODO
} // ::extend_extent()

namespace shared {

void *
alloc(local::PoolsRAII &pools, std::size_t length) noexcept {
  const auto bucketSz = shared::bucket_size_for(length);
  local::Pool &pool = shared::pool_for(pools, bucketSz);

  sp::SharedLock guard{pool.lock};
  if (guard) {

    header::Node *start = &pool.start;
    if (start) {

    reserve_start:
      void *result = ::reserve(start);
      if (result) {

        return result;
      } else {
        header::Node *const next = ::next_extent(start);
        if (next) {

          start = next;
          goto reserve_start;
        } else {

          if (::should_expand_extent(start)) {
            ::expand_extent(start);
          } else {
            start = ::enqueue_new_extent(pools, start->next, bucketSz);
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
          ::enqueue_new_extent(pools, pool.start.next, bucketSz);
      if (current) {
        void *const result = ::reserve(current);
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
} // shared::alloc()

void *
alloc(local::Pools &pools, std::size_t length) noexcept {
  assert(pools.pools);
  return alloc(*pools.pools, length);
}

} // namespace shared
