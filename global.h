#ifndef SP_MALLOC_GLOBAL_H
#define SP_MALLOC_GLOBAL_H

#include "shared.h"

#define SP_TEST
#ifdef SP_TEST
#include <tuple>
#include <vector>
#endif

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

  State() noexcept                                       //
      : brk_lock{}, brk_position{nullptr}, brk_alloc{0}, //
        free{0, nullptr} {
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

/*none Thread local free*/
bool free(void *) noexcept;

/*Alloc ThreadLocal headers*/
local::PoolsRAII *alloc_pool() noexcept;
void release_pool(local::PoolsRAII *) noexcept;

} // namespace global

#ifdef SP_TEST
// any of these can not be used during load
namespace test {
std::vector<std::tuple<void *, std::size_t>> watch_free(global::State *);
void clear_free(global::State *);
void print_free(global::State *);
std::size_t count_free(global::State *);
void sort_free(global::State *);
void coalesce_free(global::State *);
} // namespace test
#endif

#endif
