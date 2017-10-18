#include "global.h"
#include "shared.h"
#include "stuff.h"
#include <cassert>
#include <string.h>

/*
 *===========================================================
 *========SP=================================================
 *===========================================================
 */
bool
operator<(const sp::index &i, const sp::buckets &b) {
  return i.operator<(b.data);
}

sp::buckets
operator/(const sp::node_size &node, const sp::bucket_size &bucket) {
  return sp::buckets((node.operator/(bucket.data)).data);
}

std::ptrdiff_t operator*(const sp::index &idx, const sp::bucket_size &bucket) {
  return idx.operator*(bucket.data).data;
}

sp::index
operator-(const sp::index &idx, const sp::buckets &b) {
  return idx.operator-(b.data);
}

sp::index
operator+(const sp::index &idx, const sp::buckets &b) {
  return idx.operator+(b.data);
}

bool
operator>(const sp::bucket_size &bs, const sp::node_size &ns) {
  return bs.operator>(ns.data);
}

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
    : next_lock{}
    , size(sz)
    , next(nxt) {
}

bool
is_consecutive(const Free *const head, const Free *const tail) noexcept {
  assert(head != nullptr);
  assert(tail != nullptr);
  uintptr_t head_base = reinterpret_cast<uintptr_t>(head);
  uintptr_t head_end = head_base + head->size;
  uintptr_t tail_base = reinterpret_cast<uintptr_t>(tail);
  return head_end == tail_base;
} // is_consecutive()

void
coalesce(Free *head, Free *tail, Free *const next) noexcept {
  assert(is_consecutive(head, tail));
  head->size = head->size + tail->size;
  memset(tail, 0, tail->size); // TODO only debug
  head->next.store(next, std::memory_order_relaxed);
} // coalesce()

Free *
init_free(void *const head, std::size_t length) noexcept {
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

Free *
reduce(Free *free, std::size_t length) noexcept {
  assert(free->size >= length);
  assert((free->size - length) >= sizeof(Free));

  std::size_t newSz = free->size - length;
  free->size = newSz;

  void *const result = util::ptr_math(free, +newSz);
  return new (result) Free(length, nullptr);
} // reduce()

Free *
free(void *const start) noexcept {
  assert(start != nullptr);
  uintptr_t startPtr = reinterpret_cast<uintptr_t>(start);
  assert(startPtr % alignof(Free) == 0);
  return reinterpret_cast<Free *>(start);
} // free()

/*Extent*/
static_assert(sizeof(Extent) == SP_MALLOC_CACHE_LINE_SIZE, "");
static_assert(alignof(Extent) == SP_MALLOC_CACHE_LINE_SIZE, "");

Extent::Extent() noexcept //
    : reserved{false} {
}

static sp::buckets
calc_buckets(sp::node_size nodeSz, sp::bucket_size bucketSz) noexcept {
  assert(bucketSz > 0);
  assert(nodeSz > header::SIZE);

  return (nodeSz - header::SIZE) / bucketSz;
}

Extent *
extent(Node *const start) noexcept {
  assert(start != nullptr);
  assert(start->type == NodeType::HEAD);
  uintptr_t startPtr = reinterpret_cast<uintptr_t>(start);
  uintptr_t headerPtr = startPtr + sizeof(Node);
  assert(headerPtr % alignof(Extent) == 0);

  return reinterpret_cast<Extent *>(headerPtr);
} // extent()

bool
is_empty(Extent *const ext) noexcept {
  return ext->reserved.all(0);
}

/*Node*/
// static_assert(alignof(Node) == SP_MALLOC_CACHE_LINE_SIZE, "");
static_assert(sizeof(Node) == SP_MALLOC_CACHE_LINE_SIZE, "");

Node::Node(NodeType t, sp::node_size nodeSz, sp::bucket_size bucketSz,
           sp::buckets p_buckets) noexcept //
    : pad0()
    , next{nullptr}
    , bucket_size(bucketSz)
    , node_size(nodeSz)
    , buckets(p_buckets)
    , type(t)
    , pad1() {
} // Node()

Node *
init_extent(void *const raw, sp::node_size size,
            sp::bucket_size bucketSz) noexcept {
  assert(raw != nullptr);
  assert(size >= header::SIZE);
  assert(bucketSz > 0);
  const sp::buckets buckets = calc_buckets(size, bucketSz);
  assert(buckets > 0);

  Node *const nHdr = node(raw);
  new (nHdr) Node(NodeType::HEAD, size, bucketSz, buckets);

  Extent *const eHdr = extent(nHdr);
  // memset(raw, 0, length);
  new (eHdr) Extent;

  return nHdr;
} // init_node()

Node *
node(void *const start) noexcept {
  assert(start != nullptr);
  uintptr_t startPtr = reinterpret_cast<uintptr_t>(start);
  assert(startPtr % Node::ALIGNMENT == 0);
  return reinterpret_cast<Node *>(start);
} // node()

sp::node_size
node_data_size(Node *node) noexcept {
  sp::node_size result = node->node_size;
  if (node->type == NodeType::HEAD) {
    assert(result >= header::SIZE);
    result = result - header::SIZE;
  } else if (node->type == NodeType::LINK) {
    assert(result >= sizeof(Node));
    result = result - sizeof(Node);
  }
  return result;
}

std::uintptr_t
node_data_start(Node *node) noexcept {
  uintptr_t result = reinterpret_cast<uintptr_t>(node);
  result += sizeof(Node);
  if (node->type == NodeType::HEAD) {
    result += sizeof(Extent);
  }
  return result;
}
} // namespace header

/*
 *===========================================================
 *=======LOCAL===============================================
 *===========================================================
 */
namespace local {
// class Pool {{{
Pool::Pool() noexcept
    : start{header::NodeType::SPECIAL, sp::node_size(0), sp::bucket_size(0),
            sp::buckets(0)}
    , lock{} {
}
// }}}

// class PoolsRAII {{{
PoolsRAII::PoolsRAII() noexcept
    : buckets{}
    , total_alloc{0}
    , priv{nullptr}
    , next{nullptr}
    , reclaim{false} {
  // TODO std::atomic_thread_fence(std::memory_order_release);?
}

Pool &PoolsRAII::operator[](std::size_t idx) noexcept {
  assert(idx < BUCKETS);
  return buckets[idx];
}

// }}}

// class Pools {{{
Pools::Pools() noexcept
    : pools{nullptr} {
  pools = global::alloc_pool();
  printf("ctor Pools TL\n");
}

Pools::~Pools() noexcept {
  if (pools) {
    global::release_pool(pools);
    pools = nullptr;
  }
  printf("dtor Pools TL\n");
}

Pool &Pools::operator[](std::size_t idx) noexcept {
  assert(pools != nullptr);
  assert(idx < BUCKETS);
  return pools->buckets[idx];
}
// }}}

} // namespace local
