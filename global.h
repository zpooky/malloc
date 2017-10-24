#ifndef SP_MALLOC_GLOBAL_H
#define SP_MALLOC_GLOBAL_H

#include "shared.h"
#include <mutex>

/*
 *===========================================================
 *========GLOBAL=============================================
 *===========================================================
 */
namespace global {
struct State {
  // brk{{{
  std::mutex brk_lock;
  void *brk_position; // not used for now
  std::size_t brk_alloc;
  // }}}

  // free{{{
  header::Free free;
  // }}}

  State() noexcept //
      : brk_lock{}
      , brk_position{nullptr}
      , brk_alloc{0}
      , free{sp::node_size(0), nullptr} {
    // TODO move to src
    std::atomic_thread_fence(std::memory_order_release);
  }
};
namespace internal {
header::Free *
find_freex(State &, sp::node_size) noexcept;
void *
alloc(State &, sp::node_size) noexcept;
void
dealloc(State &, void *, sp::node_size) noexcept;

} // namespace internal

/*raw block alloc*/
void *alloc(sp::node_size) noexcept;
void
dealloc(void *, sp::node_size) noexcept;

} // namespace global

#endif
