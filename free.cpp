#include "free.h"
#include <cassert>

namespace shared {

int
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
        return index;
      }
      ++index;
      it += node->bucket_size;
    }
    assert(false);
  }
  return -1;
}

std::size_t
node_indecies_in(header::Node *node) noexcept {
  std::size_t result = header::node_data_size(node);
  return std::size_t(result / node->bucket_size);
}

bool
perform_free(header::Extent *ext, std::size_t idx) noexcept {
  assert(ext != nullptr);
  assert(idx < header::Extent::MAX_BUCKETS);

  if (!ext->reserved.set(idx, false)) {
    // double free is a runtime fault
    assert(false);
  }

  return true;
}

static header::Node *
find_parent(header::Node *start, header::Node *const child) {
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
unlink_extent(header::Node *parent, header::Node *head) {
}

static void
reclaim_extent(header::Node *head) {
}

enum class FreeLogic { FREED, NOT_FOUND, FREED_RECLAIMED };
static FreeLogic
free_logic(local::Pool &pool, void *const search) noexcept {
  sp::SharedLock shared_guard(pool.lock);
  if (shared_guard) {
    header::Node *parent = nullptr;
    header::Node *head = nullptr;
    std::size_t index{0};
  start:
    header::Node *current = parent->next.load(std::memory_order_acquire);
    if (current) {
      if (current->type == header::NodeType::HEAD) {
        head = current;
        index = 0;
      }

      int nodeIdx = node_index_of(current, search);
      if (nodeIdx != -1) {
        assert(head != nullptr);
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
                  reclaim_extent(head);

                  return FreeLogic::FREED_RECLAIMED;
                }
              } else {
                // should always succeed
                assert(false);
              }
            } else {
              assert(false);
              // TODO what now?
              // - retry
              // - store in NodeHead that the node should be reclaimed
              //  - how is done atomically with malloc+concurrent free?
            }
          }
        }

        return FreeLogic::FREED;
      }
      index += node_indecies_in(current);

      parent = current;
      goto start;
    }
  }

  return FreeLogic::NOT_FOUND;
} // local::extent_for()

// should return enum
// [FREED,
// NOT_FOUND,
// FREED_EXTENT_RECLAIMED/FREED_POOL_EMPTY(if we have sum size of allocs&it's 0)
// ]

enum class FeeCode { FREED, NOT_FOUND };
bool
free(local::PoolsRAII &pools, void *const ptr) noexcept {
  auto arg = nullptr;
  auto res = local::pools_find<bool, std::nullptr_t>(
      pools, ptr, //
      [](local::Pool &p, void *search, std::nullptr_t &) -> util::maybe<bool> {
        if (free_logic(p, search) != FreeLogic::NOT_FOUND) {
          return util::maybe<bool>{true};
        }
        return {};
      },
      arg);

  bool def = false;
  return res.get_or(def);
}

bool
free(local::Pools &pools, void *const ptr) noexcept {
  assert(pools.pools != nullptr);
  return free(*pools.pools, ptr);
} // free()

} // namespace shared
