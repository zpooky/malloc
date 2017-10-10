#include "free.h"
#include "global.h"
#include <cassert>
#include <cstring>

namespace shared {

template <typename Res, typename Arg>
using NodeFor = Res (*)(header::Node *, Arg &);

static bool
node_in_range(const header::Node *const node, void *const ptr) noexcept {
  assert(node);
  assert(ptr);

  const uintptr_t start = reinterpret_cast<uintptr_t>(node);
  const uintptr_t end = start + node->node_size;

  const uintptr_t compare = reinterpret_cast<uintptr_t>(ptr);
  return compare >= start && compare < end;
} // in_node_range()

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
} // node_for()

static std::int32_t
node_index_of(header::Node *node, void *ptr) noexcept {
  const std::size_t data_size = header::node_data_size(node);
  const uintptr_t data_start = header::node_data_start(node);
  const uintptr_t data_end = data_start + data_size;
  const uintptr_t search = reinterpret_cast<uintptr_t>(ptr);

  if (search >= data_start && search < data_end) {
    uintptr_t it = data_start;
    std::size_t index = 0;

    // TODO make better
    while (it < data_end) {
      if (it == search) {
        // TODO assert index < int32_t::max
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
  if (node->type != header::NodeType::SPECIAL) {
    const std::size_t result = header::node_data_size(node);

    return std::size_t(result / node->bucket_size);
  }

  return 0;
}

template <typename Res, typename Arg>
using ExtFor = Res (*)(header::Node *, std::size_t, Arg &);

template <typename Res, typename Arg>
static util::maybe<Res>
extent_for(local::Pool &pool, void *const search, ExtFor<Res, Arg> f,
           Arg &arg) noexcept {
  sp::SharedLock guard(pool.lock);
  if (guard) {
    header::Node *current = &pool.start;
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
        assert(index < header::Extent::MAX_BUCKETS);

        return util::maybe<Res>(f(extent, index, arg));
      }
      index += node_indecies_in(current);

      current = current->next.load(std::memory_order_acquire);
      goto start;
    }
  }

  return {};
} // extent_for()

static bool
perform_free(header::Extent *ext, std::size_t idx) noexcept {
  assert(ext);
  assert(idx < header::Extent::MAX_BUCKETS);

  // TODO the set function should fail if the action was made no change(someone
  // else concurrently made the same change, or a double free).
  // printf("reserved.set(%zu, false): ", idx);
  if (!ext->reserved.set(idx, false)) {
    // double free is a runtime fault
    return false;
  }
  // printf("true\n");

  return true;
}

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
}

static void
unlink_extent(header::Node *parent, header::Node *head) noexcept {
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
}

static std::size_t
recycle_extent(header::Node *head) noexcept {
  assert(head);
  // TODO local::free_list support
  std::size_t recycled(0);
start:
  header::Node *next = head->next.load();
  recycled += head->node_size;

  global::dealloc(head, head->node_size);
  if (next) {
    head = next;
    goto start;
  }
  return recycled;
}

static FreeCode
free_logic(local::Pool &pool, void *search, header::Node *&recycled) noexcept {
  sp::SharedLock shared_guard(pool.lock);
  if (shared_guard) {
    header::Node *parent = &pool.start;
    header::Node *head = nullptr;
    std::size_t index{0};
  start:
    header::Node *current = parent->next.load(std::memory_order_acquire);
    if (current) {
      if (current->type == header::NodeType::HEAD) {
        head = current;
        index = 0;
      }

      const std::int32_t nodeIdx = node_index_of(current, search);
      if (nodeIdx != -1) {
        assert(head);
        index += nodeIdx;

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
      index += node_indecies_in(current);

      parent = current;
      goto start;
    } // current
  }   /*shared*/

  return FreeCode::NOT_FOUND;
}

FreeCode
free(local::PoolsRAII &pools, void *const ptr) noexcept {
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

  // FREED_RECLAIM in this context means that the Extent was reclaimed
  if (result == FreeCode::FREED_RECLAIM) {
    assert(recycledExtent);
    std::size_t recycled = recycle_extent(recycledExtent);

    std::size_t total = pools.total_alloc.fetch_sub(recycled);
    if (total == recycled) {
      if (pools.reclaim.load()) {
        // TODO recycle local::free_list

        // FREE_RECLAIM in this context means Pool can be reclaimed
        return FreeCode::FREED_RECLAIM;
      }
    }
  }

  return FreeCode::FREED;
} // shared::free()

FreeCode
free(local::Pools &pools, void *const ptr) noexcept {
  assert(pools.pools);
  return free(*pools.pools, ptr);
} // shared::free()

util::maybe<std::size_t>
usable_size(local::PoolsRAII &pools, void *const ptr) noexcept {
  using Arg = std::nullptr_t;
  Arg arg = nullptr;

  auto res = local::pools_find<std::size_t, Arg>(
      pools, ptr, //
      [](local::Pool &pool, void *search,
         Arg &a) -> util::maybe<std::size_t> { //
        return node_for<std::size_t, Arg>(
            pool, search, //
            [](header::Node *current, Arg &) -> std::size_t {
              //
              return current->bucket_size;
            },
            a);
      },
      arg);
  return res;
} // shared::usable_size()

util::maybe<std::size_t>
usable_size(local::Pools &pools, void *const ptr) noexcept {
  assert(pools.pools);
  return usable_size(*pools.pools, ptr);
} // shared::usable_size()

util::maybe<void *>
realloc(local::PoolsRAII &pools, void *const ptr, std::size_t length) noexcept {
  using Arg = std::tuple<void *, std::size_t>;
  Arg arg(ptr, length);

  return local::pools_find<void *, Arg>(
      pools, ptr, //
      [](local::Pool &pool, void *search, Arg &arg) {
        return extent_for<void *, Arg>(
            pool, search, //
            [](header::Node *head, std::size_t idx, Arg &arg) {

              std::size_t length = std::get<1>(arg);
              void *ptr = std::get<0>(arg);

              header::Extent *ext = header::extent(head);
              if (head->bucket_size < length) {

                void *nptr = malloc(length); // TODO deadlock!
                if (nptr) {
                  memcpy(nptr, ptr, head->bucket_size);
                }
                perform_free(ext, idx);

                return nptr;
              }

              return ptr;
            },
            arg);
      },
      arg);
}

util::maybe<void *>
realloc(local::Pools &pools, void *const ptr, std::size_t length) noexcept {
  assert(pools.pools);
  return realloc(*pools.pools, ptr, length);
} // shared::realloc()

} // namespace shared
