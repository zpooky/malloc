#ifndef SP_MALLOC_FREE_H
#define SP_MALLOC_FREE_H

#include "shared.h"

namespace shared {
// TODO move to src

int
node_index_of(header::Node *node, void *ptr) noexcept;

std::size_t
node_indecies_in(header::Node *node) noexcept;

template <typename Res, typename Arg>
using ExtFor = Res (*)(header::Node *, std::size_t, Arg &);

template <typename Res, typename Arg>
static util::maybe<Res>
extent_for(local::Pool &pool, void *const search, ExtFor<Res, Arg> f,
           Arg &arg) noexcept {
  sp::SharedLock guard(pool.lock);
  if (guard) {
    header::Node *current = pool.start.load(std::memory_order_acquire);
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

        return util::maybe<Res>(f(extent, index, arg));
      }
      index += node_indecies_in(current);

      current = current->next.load(std::memory_order_acquire);
      goto start;
    }
  }

  return {};
} // local::extent_for()

bool
perform_free(header::Extent *ext, std::size_t idx) noexcept;

bool
free(local::PoolsRAII &pools, void *const ptr) noexcept ;

bool free(local::Pools &pools, void *const ptr) noexcept;
}

#endif
