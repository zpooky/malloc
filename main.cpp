#include <atomic>
#include <cassert>
#include <cstdio>
#include <iostream>
#include <mutex>

#include <algorithm>
#include <utility>

#include "bitset/Bitset.h"
#include <unistd.h> //sbrk

#include "main.h"

/*
 *===========================================================
 *=======GLOBAL==============================================
 *===========================================================
 */
namespace global {

namespace internal {
struct State {
  // atomic_list<Pair<void*,length>> free_space;
  // atomic_list<Pair<void*,length>> pending_reclaim;
  // std::atomic<void *> position;

  std::mutex brk_lock;
  void *brk_position; // not used for now
  std::size_t brk_alloc;

  State() noexcept //
      : brk_lock{},
        brk_position{nullptr},
        brk_alloc{0} {
  }
};

static State state;

static std::pair<void *, size_t> find_free(size_t, size_t) noexcept {
  return std::make_pair(nullptr, 0);
}

static std::pair<void *, size_t> new_free(size_t atLeast, size_t) noexcept {
  void *res = nullptr;
  {
    std::lock_guard<std::mutex> guard(state.brk_lock);
    // if (state.brk_position == nullptr) {
    //   state.brk_position = ::sbrk(0);
    // }
    // TODO some algorithm to determine optimal alloc size
    std::size_t allocSz = std::max(state.brk_alloc, SP_ALLOC_INITIAL_ALLOC);
    allocSz = std::max(atLeast, allocSz);
    // TODO check wrap around

    // void *newPos = state.brk_position + allocSz;
    res = ::sbrk(allocSz);
    if (res != (void *)-1) {
      state.brk_alloc = state.brk_alloc + allocSz;
      return std::make_pair(res, allocSz);
    }
  }

  return std::make_pair(nullptr, 0);
}

static void return_free(const std::pair<void *, size_t> &free) noexcept {
  if (std::get<1>(free) != 0) {
  }
}

} // namespace internal

static void *alloc(size_t sz, size_t align) noexcept {
  auto free = internal::find_free(sz, align);
  if (std::get<0>(free) == nullptr) {
    free = internal::new_free(sz, align);
    if (std::get<0>(free) == nullptr) {
      return nullptr;
    }
  }
  // TODO
  void *result = 0;
  internal::return_free(free);
  return result;
}

} // namespace global

namespace {

void *allign(void *, std::uint32_t alignment) {
  assert(alignment % 2 == 0);
  return nullptr;
}

// enum class BlockSize : uint8_t {
//   BYTE8,
//   BYTE16,
//   BYTE32,
//   BYTE64,
//   BYTE128,
//   BYTE256 = 256
// };

} // namespace

/*
 *===========================================================
 *=======LOCAL===============================================
 *===========================================================
 */
namespace local {
static thread_local Pools pool;

} // namespace local

/*
 *===========================================================
 *=======PUBLIC==============================================
 *===========================================================
 */

void *sp_malloc(std::size_t sz) {
  // std::size_t allocSz = round_even(sz);
  // Pool& pool = pool_for(local::pool, allocSz);
  return nullptr;
}

void sp_free(void *) {
}

int main() {
  // init();
}
