#include "alloc.h"
#ifdef SP_TEST
#include "alloc_debug.h"
#endif

#include "ReadWriteLock.h"
#include "bitset/Bitset.h"
#include "global.h"
#include <cstring>

static void *
local_alloc(header::LocalFree &base, sp::node_size sz) noexcept {
/* Exclusive Lock only when dequeuing.
 * Shared Lock when enqueueing & shrinking Free entry.
 * Single dequeue(*alloc) multiple enqueue(free).
 * dubble linked but not circular.
 */
// TODO coalesce
start : {
  header::LocalFree *best = nullptr;
  sp::SharedLock guard{base.lock};
  if (guard) {
    header::LocalFree *current = base.next;
  next:
    if (current) {
      if (current->size == sz) {
        sp::TryPrepareLock pre_guard{guard};
        if (pre_guard) {
          sp::EagerExclusiveLock ex_guard{pre_guard};
          if (ex_guard) {

            if (current->next)
              current->next->priv = current->priv;

            if (current->priv)
              current->priv->next = current->next;

            if (base.next == current)
              base.next = current->next;

#ifdef SP_TEST
            std::memset(current, 0, std::size_t(current->size));
#endif
            return current;
          } else {
            assert(false);
          }
        } else {
          goto start;
        }
      } else {
        if (current->size > sz) {
          if (!best || best->size > current->size) {
            best = current;
          }
        }
        current = current->next;
        goto next;
      }
    } else if (best) {
      return header::reduce(best, sz);
    }
  } else {
    // TODO handle
    assert(false);
  }
}
  return nullptr;
} // ::local_alloc()

static void *
alloc(local::PoolsRAII &pools, sp::node_size sz) noexcept {
  void *result = local_alloc(pools.base_free, sz);
  if (!result) {
    // TODO logic to allocate extra memory for locall::FreeList
    result = global::alloc(sz);
    if (result) {
      pools.total_alloc.fetch_add(std::size_t(sz));
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

#ifdef SP_TEST

namespace debug {
std::size_t
alloc_count_alloc(local::PoolsRAII &p) {
  std::size_t result(0);
  for (std::size_t i(8); i > 0; i <<= 1) {
    result += alloc_count_alloc(p, i);
  }
  return result;
}

static std::size_t
count_reserved(header::Extent &ext, sp::buckets buckets) {
  std::size_t result(0);
  auto &b = ext.reserved;
  for (std::size_t idx(0); idx < std::size_t(buckets); ++idx) {
    if (b.test(idx)) {
      result++;
    }
  }

  return result;
}

std::size_t
alloc_count_alloc(local::PoolsRAII &p, std::size_t sz) {
  std::size_t result(0);
  sp::bucket_size bsz = shared::bucket_size_for(sz);
  local::Pool &pool = shared::pool_for(p, bsz);

  sp::SharedLock guard(pool.lock);
  if (guard) {
    header::Node *current = pool.start.next.load();
  start:
    if (current) {
      assert(current->type == header::NodeType::HEAD);
      auto ext = header::extent(current);
      assert(ext != nullptr);
      result += count_reserved(*ext, current->buckets);

      current = current->next.load(); // TODO next_extent
      goto start;
    }
  }
  return result;
}

} // namespace debug
#endif