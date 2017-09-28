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
  // brk
  // {{{
  std::mutex brk_lock;
  void *brk_position; // not used for now
  std::size_t brk_alloc;
  // }}}

  // free
  // {{{
  header::Free free;
  // }}}

  State() noexcept //
      : brk_lock{}
      , brk_position{nullptr}
      , brk_alloc{0}
      , //
      free{0, nullptr} {
    // TODO move to src
    std::atomic_thread_fence(std::memory_order_release);
  }
};
namespace internal {
header::Free *find_freex(State &, std::size_t) noexcept;
void *alloc(State &, std::size_t) noexcept;
void dealloc(State &state, void *const start, std::size_t length) noexcept;

} // namespace internal

/*raw block alloc*/
void *alloc(std::size_t) noexcept;
void dealloc(void *, std::size_t) noexcept;

} // namespace global

#endif
