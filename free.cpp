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

  if (!ext->reserved.set(idx, false)) {
    // double free is a runtime fault
    assert(false);
  }
  // TODO reclaim node?
  return true;
}

bool
free(local::PoolsRAII &pools, void *const ptr) noexcept {
  auto arg = nullptr;
  auto res = local::pools_find<bool, std::nullptr_t>(
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
}

bool
free(local::Pools &pools, void *const ptr) noexcept {
  assert(pools.pools != nullptr);
  return free(*pools.pools, ptr);
} // free()

} // namespace shared
