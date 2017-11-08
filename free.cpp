#include "alloc.h"
#include "free.h"
#include "global.h"
#include "util.h"
#include <cassert>
#include <cstring>

using shared::FreeCode;

template <typename Res, typename Arg>
using NodeFor = Res (*)(header::Node *, Arg &);

static bool
node_in_range(const header::Node *const node, void *const ptr) noexcept {
  assert(node);
  assert(ptr);

  const uintptr_t start = reinterpret_cast<uintptr_t>(node);
  const uintptr_t end = start + std::size_t(node->node_size);

  const uintptr_t compare = reinterpret_cast<uintptr_t>(ptr);
  return compare >= start && compare < end;
} // ::node_in_range()

template <typename Res, typename Arg>
static util::maybe<Res>
node_for(local::Pool &pool, void *search, NodeFor<Res, Arg> f,
         Arg &a) noexcept {
  sp::SharedLock guard(pool.lock);
  if (guard) {
    header::Node *current = &pool.start;
  start:
    if (current) {
      if (node_in_range(current, search)) {
        return util::maybe<Res>(f(current, a));
      }
      current = current->next.load(std::memory_order_acquire);
      goto start;
    }
  }
  return {};
} // ::node_for()

static util::maybe<std::size_t>
node_index_of(header::Node *const node, void *const ptr) noexcept {
  const sp::node_size data_size = header::node_data_size(node);
  const std::uintptr_t data_start = header::node_data_start(node);
  const std::uintptr_t data_end = data_start + std::size_t(data_size);
  const std::uintptr_t search = reinterpret_cast<uintptr_t>(ptr);

  if (search >= data_start && search < data_end) {
    std::uintptr_t it = data_start;
    std::size_t index = 0;

    // TODO make better
    while (it < data_end) {
      if (it == search) {
        // TODO assert index < int32_t::max
        return util::maybe<std::size_t>(index);
      }
      ++index;
      it += std::size_t(node->bucket_size);
    }
    assert(false);
  }
  return {};
} //:node_index_of()

static sp::buckets
node_buckets(header::Node *const node) noexcept {
  if (node->type != header::NodeType::SPECIAL) {
    const sp::node_size result = header::node_data_size(node);

    return result / node->bucket_size;
  }

  return sp::buckets(0);
} //::node_buckets()

template <typename Res, typename Arg>
using ExtFor = Res (*)(header::Node *, sp::index, Arg &);

template <typename Res, typename Arg>
static util::maybe<Res>
extent_for(local::Pool &pool, void *const search, ExtFor<Res, Arg> f,
           Arg &arg) noexcept {
  sp::SharedLock guard(pool.lock);
  if (guard) {
    header::Node *current = &pool.start;
    header::Node *extent = nullptr;
    sp::index index{0};
  start:
    if (current) {
      if (current->type == header::NodeType::HEAD) {
        extent = current;
        index = std::size_t(0);
      }

      auto nodeIdx = node_index_of(current, search);
      if (nodeIdx) {
        assert(extent != nullptr);
        index = index + nodeIdx.get();
        assert(index < header::Extent::MAX_BUCKETS);

        return util::maybe<Res>(f(extent, index, arg));
      }
      index = index + node_buckets(current);

      current = current->next.load(std::memory_order_acquire);
      goto start;
    }
  }

  return {};
} // ::extent_for()

static bool
perform_free(header::Extent *ext, sp::index idx) noexcept {
  assert(ext);
  assert(idx < header::Extent::MAX_BUCKETS);

  if (!ext->reserved.set(std::size_t(idx), false)) {
    // double free is a runtime fault
    return false;
  }

  return true;
} // ::perform_free()

static header::Node *
find_parent(header::Node *start, header::Node *const child) noexcept {
start:
  if (start) {
    header::Node *tmp = start->next.load();
    if (tmp == child) {
      return start;
    }
    start = tmp;
    goto start;
  }

  assert(false);
  return nullptr;
} //::find_parent()

static void
unlink_extent(header::Node *const parent, header::Node *head) noexcept {
  assert(parent);
  assert(head);
start:
  header::Node *current = head->next.load(std::memory_order_acquire);
  if (current) {
    if (current->type == header::NodeType::LINK) {
      head = current;
      goto start;
    }
    head->next.store(nullptr, std::memory_order_relaxed);
  }
  parent->next.store(current);
} //::unlink_extent()

namespace local {
static void
dealloc(local::PoolsRAII &ps, header::LocalFree *first,
        header::LocalFree *last) noexcept {
  sp::SharedLock guard{ps.free_lock};
  if (guard) {
    // Not a double linked list when in the free stack
    auto base = ps.free_stack.load();
  retry:
    last->next = base;
    if (!ps.free_stack.compare_exchange_weak(base, first)) {
      goto retry;
    }
  } else {
    // TODO handle
    assert(false);
  }
} // local::dealloc()

} // namespace local

static std::size_t
recycle_extent(local::PoolsRAII &tl, local::PoolsRAII &ps,
               header::Node *head) noexcept {
  assert(head);

  std::size_t recycled(0);
  header::LocalFree *first = nullptr;
  header::LocalFree *last = nullptr;
start:
  if (head) {
    header::Node *next = head->next.load();
    recycled += std::size_t(head->node_size);

    first = header::init_local_free(head, head->node_size, first);
    if (!last) {
      last = first;
    }

    head = next;
    goto start;
  }

  if (ps.reclaim.load()) {
    if (tl.reclaim.load()) {
      global::dealloc(first);
    } else {
      local::dealloc(tl, first, last);
    }
  } else {
    local::dealloc(ps, first, last);
  }
  return recycled;
} //::recycle_extent()

static FreeCode
free_logic(local::Pool &pool, void *search, header::Node *&recycled) noexcept {
  sp::SharedLock shared_guard(pool.lock);
  if (shared_guard) {
    header::Node *parent = &pool.start;
    header::Node *head = nullptr;
    sp::index index{0};
  start:
    header::Node *current = parent->next.load(std::memory_order_acquire);
    if (current) {
      if (current->type == header::NodeType::HEAD) {
        head = current;
        index = 0;
      }

      auto nodeIdx = node_index_of(current, search);
      if (nodeIdx) {
        assert(head);
        index = index + nodeIdx.get();

        header::Extent *const extent = header::extent(head);
        if (perform_free(extent, index)) {
          if (header::is_empty(extent)) {

            sp::TryPrepareLock pre_guard(shared_guard);
            if (pre_guard) {
              sp::EagerExclusiveLock ex_guard(pre_guard);
              if (ex_guard) {
                if (header::is_empty(extent)) {
                  // we need to consider that malloc can insert new extent and
                  // nodes concurrently when this is performed. This means that
                  // parent read during the shared lock can be different now
                  // during the exclusive lock. Solve this by iterating from
                  // parent until head is next, this work since during an
                  // exclusive lock there can not be any new extents or nodes.

                  parent = find_parent(parent, head);
                  unlink_extent(parent, head);
                  recycled = head;

                  return FreeCode::FREED_RECLAIM;
                } /*is_empty*/
              } /*exclusive*/ else {
                // should always succeed
                assert(false);
              }
            } /*prepare*/ else {
              assert(false);
              // TODO what now?
              // - retry
              // - store in NodeHead that the node should be reclaimed
              //  - set extent_reclaim true on the head node
              //  - malloc thread sees extent_reclaim and ignores the extent
              //  - freeing thread sees extent_reclaim and checks
              //    is_empty(extent) if not true unset flag, if true free to
              //    reclaim
            }
          } /*is_empty*/

          return FreeCode::FREED;
        } /*perform_free*/

        return FreeCode::DOUBLE_FREE;
      } // nodeIdx
      index = index + node_buckets(current);

      parent = current;
      goto start;
    } // current
  }   /*shared*/

  return FreeCode::NOT_FOUND;
} //::free_logic()

static FreeCode
free_reclaim(local::PoolsRAII &tl, local::PoolsRAII &ps, FreeCode c,
             header::Node *rExts) noexcept {
  // FREED_RECLAIM in this context means that the Extent was reclaimed
  if (c == FreeCode::FREED_RECLAIM) {
    assert(rExts);
    const std::size_t recycled = recycle_extent(tl, ps, rExts);

    std::size_t total = ps.total_alloc.fetch_sub(recycled);
    if (total == recycled) {
      if (ps.reclaim.load()) {
        // FREE_RECLAIM in this context means Pool can be reclaimed
        return FreeCode::FREED_RECLAIM;
      }
    }
  }

  return FreeCode::FREED;
} //::free_reclaim()

static FreeCode
free(local::PoolsRAII &tl, local::PoolsRAII &pools, void *ptr,
     sp::bucket_size length) noexcept {
  local::Pool &pool = shared::pool_for(pools, length);

  header::Node *recycled_ext = nullptr;
  auto result = free_logic(pool, ptr, recycled_ext);
  if (result == FreeCode::NOT_FOUND || result == FreeCode::DOUBLE_FREE) {
    return result;
  }

  return free_reclaim(tl, pools, result, recycled_ext);
} //::free()

namespace shared {

FreeCode
free(local::PoolsRAII &tl, local::PoolsRAII &pools, void *const ptr) noexcept {
  assert(ptr);
  header::Node *recycledExtent = nullptr;
  auto res = local::pools_find<FreeCode, header::Node *>(
      pools, ptr, //
      [](local::Pool &p, void *search,
         header::Node *&recycled) -> util::maybe<FreeCode> {
        auto result = free_logic(p, search, recycled);
        if (result == FreeCode::NOT_FOUND) {
          return {};
        }

        return util::maybe<FreeCode>(result);
      },
      recycledExtent);

  FreeCode result = res.get_or(FreeCode::NOT_FOUND);
  if (result == FreeCode::NOT_FOUND || result == FreeCode::DOUBLE_FREE) {
    return result;
  }

  return free_reclaim(tl, pools, result, recycledExtent);
} // shared::free()

FreeCode
free(local::Pools &tl, local::Pools &pools, void *const ptr) noexcept {
  assert(tl.pools);
  assert(pools.pools);
  return free(*tl.pools, *pools.pools, ptr);
} // shared::free()

util::maybe<sp::bucket_size>
usable_size(local::PoolsRAII &pools, void *const ptr) noexcept {
  using Arg = std::nullptr_t;
  Arg arg = nullptr;

  auto res = local::pools_find<sp::bucket_size, Arg>(
      pools, ptr, //
      [](local::Pool &pool, void *search,
         Arg &a) -> util::maybe<sp::bucket_size> { //
        return node_for<sp::bucket_size, Arg>(
            pool, search, //
            [](header::Node *current, Arg &) -> sp::bucket_size {
              return current->bucket_size;
            },
            a);
      },
      arg);
  return res;
} // shared::usable_size()

util::maybe<sp::bucket_size>
usable_size(local::Pools &pools, void *const ptr) noexcept {
  assert(pools.pools);
  return usable_size(*pools.pools, ptr);
} // shared::usable_size()

util::maybe<void *>
realloc(local::PoolsRAII &tl, local::PoolsRAII &pools, void *ptr,
        std::size_t length, /*OUT*/ FreeCode &result) noexcept {
  auto maybeMemSz = usable_size(pools, ptr);
  if (maybeMemSz) {
    sp::bucket_size memSz = maybeMemSz.get();
    if (memSz < length) {
      void *const nptr = alloc(tl, length);
      if (nptr) {
        std::memcpy(nptr, ptr, std::size_t(memSz));
      } else {
        // runtime fault, out of memory
        assert(false);
      }
      result = ::free(tl, pools, ptr, memSz);
      assert(result == FreeCode::FREED || result == FreeCode::FREED_RECLAIM);

      ptr = nptr;
    }
    return util::maybe<void *>(ptr);
  }

  result = FreeCode::NOT_FOUND;
  return {};
}

util::maybe<void *>
realloc(local::Pools &tl, local::Pools &pools, void *const ptr,
        std::size_t length, /*OUT*/ FreeCode &code) noexcept {
  assert(tl.pools);
  assert(pools.pools);
  return realloc(*tl.pools, *pools.pools, ptr, length, code);
} // shared::realloc()

} // namespace shared
