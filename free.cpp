#include "free.h"
#include "global.h"
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
  if (node->type != header::NodeType::SPECIAL) {
    std::size_t result = header::node_data_size(node);

    return std::size_t(result / node->bucket_size);
  }

  return std::size_t(0);
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
recycle_extent(header::Node *head) {
  // TODO This can and should be done when the pool lock is not held.
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

enum class FreeLogic { FREED, NOT_FOUND, FREED_RECLAIMED };
static FreeLogic
free_logic(local::Pool &pool, void *const search) noexcept {
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
                  std::size_t recycled = recycle_extent(head);

                  // TODO return size of recycled nodes & decrement sum of
                  // allocated
                  return FreeLogic::FREED_RECLAIMED;
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
        }   /*perform_free*/

        return FreeLogic::FREED;
      }
      index += node_indecies_in(current);

      parent = current;
      goto start;
    }
  } /*shared*/

  return FreeLogic::NOT_FOUND;
}

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
