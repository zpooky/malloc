#include "free.h"

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

static bool
free_logic(local::Pool &pool, void *const search) noexcept {
  sp::SharedLock shared_guard(pool.lock);
  if (shared_guard) {
    header::Node *current = pool.start.load(std::memory_order_acquire);
    header::Node *head = nullptr;
    std::size_t index{0};
  start:
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
                }
              } else {
                // should always succeed
                assert(false);
              }
            } else {
              // TODO what now?
              // - retry
              // - store in NodeHead that the node should be reclaimed
              //  - how is done atomically with malloc+concurrent free?
            }
          }
        }
        return true;
      }
      index += node_indecies_in(current);

      current = current->next.load(std::memory_order_acquire);
      goto start;
    }
  }
  return false;
} // local::extent_for()

bool
free(local::PoolsRAII &pools, void *const ptr) noexcept {
  auto arg = nullptr;
  auto res = local::pools_find<bool, std::nullptr_t>(
      pools, ptr, //
      [](local::Pool &p, void *search, std::nullptr_t &) -> util::maybe<bool> {
        if (free_logic(p, search)) {
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
