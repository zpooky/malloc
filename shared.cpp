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

/*Free*/
static_assert(sizeof(Free) == SP_MALLOC_CACHE_LINE_SIZE, "");
static_assert(alignof(Free) == SP_MALLOC_CACHE_LINE_SIZE, "");

Free::Free(std::size_t sz, Free *nxt) noexcept //
    : next_lock{}, size(sz), next(nxt) {
}

bool is_consecutive(const Free *const head, const Free *const tail) noexcept {
  assert(head != nullptr);
  assert(tail != nullptr);
  uintptr_t head_base = reinterpret_cast<uintptr_t>(head);
  uintptr_t head_end = head_base + head->size;
  uintptr_t tail_base = reinterpret_cast<uintptr_t>(tail);
  return head_end == tail_base;
} // is_consecutive()

void coalesce(Free *head, Free *tail, Free *const next) noexcept {
  assert(is_consecutive(head, tail));
  head->size = head->size + tail->size;
  memset(tail, 0, tail->size); // TODO only debug
  head->next.store(next, std::memory_order_relaxed);
} // coalesce()

Free *init_free(void *const head, std::size_t length) noexcept {
  if (head && length > 0) {
    assert(reinterpret_cast<uintptr_t>(head) % alignof(Free) == 0);
    assert(length >= sizeof(Free));
    memset(head, 0, length); // TODO only debug

    Free *const result = new (head) Free(length, nullptr);
    assert(reinterpret_cast<Free *>(result) == head);
    return result;
  }
  return nullptr;
} // init_free()

Free *reduce(Free *free, std::size_t length) noexcept {
  assert(free->size >= length);
  assert((free->size - length) >= sizeof(Free));

  std::size_t newSz = free->size - length;
  free->size = newSz;

  void *const result = util::ptr_math(free, +newSz);
  return new (result) Free(length, nullptr);
} // reduce()

Free *free(void *const start) noexcept {
  assert(start != nullptr);
  uintptr_t startPtr = reinterpret_cast<uintptr_t>(start);
  assert(startPtr % alignof(Free) == 0);
  return reinterpret_cast<Free *>(start);
} // free()

/*Extent*/
static_assert(sizeof(Extent) == SP_MALLOC_CACHE_LINE_SIZE, "");
static_assert(alignof(Extent) == SP_MALLOC_CACHE_LINE_SIZE, "");

Extent::Extent() noexcept //
    : reserved(false) {
}

static std::size_t calc_buckets(std::size_t extentSz,
                                std::size_t bucketSz) noexcept {
  assert(bucketSz > 0);
  assert(extentSz > header::SIZE);

  return (extentSz - header::SIZE) / bucketSz;
}

Extent *extent(Node *const start) noexcept {
  assert(start != nullptr);
  assert(start->type == NodeType::HEAD);
  uintptr_t startPtr = reinterpret_cast<uintptr_t>(start);
  uintptr_t headerPtr = startPtr + sizeof(Node);
  assert(headerPtr % alignof(Extent) == 0);

  return reinterpret_cast<Extent *>(headerPtr);
} // extent()

/*Node*/
static_assert(alignof(Node) == SP_MALLOC_CACHE_LINE_SIZE, "");
static_assert(sizeof(Node) == SP_MALLOC_CACHE_LINE_SIZE, "");

Node::Node(std::size_t nodeSz, std::size_t bucketSz,
           std::size_t p_buckets) noexcept //
    : type(NodeType::HEAD), next{nullptr}, bucket_size(bucketSz),
      node_size(nodeSz), buckets(p_buckets) {
  assert(this->bucket_size > 0);
  assert(this->node_size > 0);
  assert(this->buckets > 0);
} // Node()

Node *init_node(void *const raw, std::size_t size,
                std::size_t bucketSz) noexcept {
  assert(raw != nullptr);
  assert(size >= header::SIZE);
  const std::size_t buckets = calc_buckets(size, bucketSz);
  assert(buckets > 0);
  // printf("init_node(ptr,size(%zu),bucketSz(%zu),buckets(%zu))\n", //
  // size, bucketSz, buckets);

  Node *const nHdr = node(raw);
  new (nHdr) Node(size, bucketSz, buckets);
  // printf("node(size(%zu),bucketSz(%zu),buckets(%zu))\n", //
  //        nHdr->node_size, nHdr->bucket_size, nHdr->buckets);

  Extent *const eHdr = extent(nHdr);
  // memset(raw, 0, length);
  new (eHdr) Extent;

  return nHdr;
} // init_node()

Node *node(void *const start) noexcept {
  assert(start != nullptr);
  uintptr_t startPtr = reinterpret_cast<uintptr_t>(start);
  assert(startPtr % alignof(Node) == 0);
  return reinterpret_cast<Node *>(start);
} // node()

} // namespace header
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
} // align_pointer()

std::size_t round_even(std::size_t v) noexcept {
  if (v <= 8) {
    return 8;
  }
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
} // round_even()

void *ptr_math(void *const ptr, std::int64_t add) noexcept {
  assert(ptr != nullptr);
  uintptr_t start = reinterpret_cast<uintptr_t>(ptr);
  return reinterpret_cast<void *>(start + add);
} // ptr_math()

ptrdiff_t ptr_diff(void *const first, void *const second) noexcept {
  assert(first != nullptr);
  assert(first != nullptr);
  uintptr_t firstPtr = reinterpret_cast<uintptr_t>(first);
  uintptr_t secondPtr = reinterpret_cast<uintptr_t>(second);
  return firstPtr - secondPtr;
} // ptr_diff()

std::size_t trailing_zeros(std::size_t n) noexcept {
  return __builtin_ctz(n);
} // util::trailing_zeroes()

std::size_t leading_zeros(std::size_t n) noexcept {
  return __builtin_clz(n);
} // util:leading_zeroes()

std::size_t round_up(std::size_t data, std::size_t evenMultiple) noexcept {
  const std::size_t remaining = data % evenMultiple;
  const std::size_t add = remaining > 0 ? evenMultiple - remaining : 0;
  return data + add;
}

} // namespace util

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
