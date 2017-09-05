#include "shared.h"
#include "global.h"
#include <cassert>
#include <string.h>

/*
 *===========================================================
 *========HEADER=============================================
 *===========================================================
 */
namespace header {

void debug_print_free(Free *const head) {
  if (head) {

    void *t = reinterpret_cast<void *>(head);
    printf("[%p,%zu]", t, head->size);
    debug_print_free(head->next.load());
  } else {
    printf("\n");
  }
}

bool is_consecutive(const Free &head, const Free &tail) noexcept {
  uintptr_t head_base = reinterpret_cast<uintptr_t>(&head);
  uintptr_t head_end = head_base + head.size;
  uintptr_t tail_base = reinterpret_cast<uintptr_t>(&tail);
  return head_end == tail_base;
}

void coalesce(Free &head, const Free &tail) noexcept {
  assert(is_consecutive(head, tail));
  head.size += tail.size;
}

Free *init_free(void *const head, std::size_t length,
                header::Free *const next) noexcept {
  if (head && length > 0) {
    assert(reinterpret_cast<uintptr_t>(head) % alignof(Free) == 0);
    assert(length >= sizeof(Free));
    memset(head, 0, length); // TODO only debug

    Free *const result = new (head) Free(length, next);
    assert(reinterpret_cast<Free *>(result) == head);
    return result;
  }
  return nullptr;
}

Extent *init_extent(void *const raw, std::size_t bucket,
                    std::size_t length) noexcept {
  assert(raw != nullptr);
  // TODO calc based on bucket and nodeSz
  std::size_t extentIdxs = 0;

  Node *nHdr = node(raw);
  new (nHdr) Node(length, bucket, extentIdxs);

  Extent *eHdr = extent(raw);
  // memset(raw, 0, length);
  return new (eHdr) Extent;
} // init_extent()

Free *free(void *const start) noexcept {
  assert(start != nullptr);
  uintptr_t startPtr = reinterpret_cast<uintptr_t>(start);
  assert(startPtr % alignof(Free) == 0);
  return reinterpret_cast<Free *>(start);
}

Node *node(void *const start) noexcept {
  assert(start != nullptr);
  uintptr_t startPtr = reinterpret_cast<uintptr_t>(start);
  assert(startPtr % alignof(Node) == 0);
  return reinterpret_cast<Node *>(start);
} // node_header()

Extent *extent(void *const start) noexcept {
  assert(start != nullptr);
  uintptr_t startPtr = reinterpret_cast<uintptr_t>(start);
  uintptr_t headerPtr = startPtr + sizeof(Node);
  assert(headerPtr % alignof(Extent) == 0);

  return reinterpret_cast<Extent *>(headerPtr);
} // extent_header()
}
/*
 *===========================================================
 *========UTIL===============================================
 *===========================================================
 */
namespace util {

void *align_pointer(void *const start, std::uint32_t alignment) noexcept {
  assert(start != nullptr);
  assert(alignment >= 8);
  assert(alignment % 8 == 0);
  uintptr_t ptr = reinterpret_cast<uintptr_t>(start);
  uintptr_t diff = ptr + alignment - 1;
  ptr += diff & alignment;
  return reinterpret_cast<void *>(ptr);
}

std::size_t round_even(std::size_t v) noexcept {
  // TODO support 64 bit word
  // https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
  // 8,16,32,64,...
  v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v++;
  return v;
}

void *ptr_math(void *const ptr, std::int64_t add) noexcept {
  assert(ptr != nullptr);
  uintptr_t start = reinterpret_cast<uintptr_t>(ptr);
  return reinterpret_cast<void *>(start + add);
}

ptrdiff_t ptr_diff(void *const first, void *const second) noexcept {
  assert(first != nullptr);
  assert(first != nullptr);
  uintptr_t firstPtr = reinterpret_cast<uintptr_t>(first);
  uintptr_t secondPtr = reinterpret_cast<uintptr_t>(second);
  return firstPtr - secondPtr;
}

} // namespace

/*
 *===========================================================
 *=======LOCAL===============================================
 *===========================================================
 */
namespace local {
// class Pool {{{
Pool::Pool() noexcept //
    : start{nullptr}, lock{} {
}
// }}}

// class PoolsRAII {{{
PoolsRAII::PoolsRAII() noexcept //
    : buckets{} {
  std::atomic_thread_fence(std::memory_order_release);
}
// }}}

// class Pools {{{
Pools::Pools() noexcept //
    : pools{nullptr}, reclaimed{false} {
  pools = global::alloc_pool();
}

Pools::~Pools() noexcept {
  if (pools) {
    global::release_pool(pools);
    pools = nullptr;
  }
}

Pool &Pools::operator[](std::size_t idx) noexcept {
  return pools->buckets[idx];
}
// }}}

} // namespace local
