#include "global.h"
#include "shared.h"
#include "stuff.h"
#include <cassert>
#include <cstring>

//========sp=============================================
#ifdef SP_TYPED_NUMERIC
bool
operator<(const sp::index &i, const sp::buckets &b) {
  return i.operator<(b.data);
}

sp::buckets
operator/(const sp::node_size &n, const sp::bucket_size &b) {
  return sp::buckets((n.operator/(b.data)).data);
}

std::ptrdiff_t operator*(const sp::index &idx, const sp::bucket_size &b) {
  return idx.operator*(b.data).data;
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
operator>(const sp::bucket_size &b, const sp::node_size &n) {
  return b.operator>(n.data);
}
#endif

//========HEADER=============================================
namespace header {

/*Free*/
static_assert(sizeof(Free) == SP_MALLOC_CACHE_LINE_SIZE, "");
static_assert(alignof(Free) == SP_MALLOC_CACHE_LINE_SIZE, "");

Free::Free(sp::node_size sz, Free *nxt) noexcept
    : next_lock{}
    , size(sz)
    , next(nxt) {
}

Free::Free(sp::node_size sz) noexcept
    : Free(sz, nullptr) {
}
template <typename T>
static bool
internal_is_consecutive(const T *const head, const T *const tail) noexcept {
  assert(head);
  assert(tail);
  uintptr_t head_start = reinterpret_cast<uintptr_t>(head);
  uintptr_t head_end = head_start + std::size_t(head->size);
  uintptr_t tail_start = reinterpret_cast<uintptr_t>(tail);
#ifdef SP_TEST
  uintptr_t tail_end = tail_start + std::size_t(tail->size);
  assert(!(head_end > tail_start && head_end < tail_end));
  assert(!(tail_end > head_start && tail_end < head_end));
#endif
  return head_end == tail_start;
}

bool
is_consecutive(const Free *const head, const Free *const tail) noexcept {
  return internal_is_consecutive(head, tail);
} // header::is_consecutive()

void
coalesce(Free *head, Free *tail, Free *const next) noexcept {
  assert(is_consecutive(head, tail));
  head->size = head->size + tail->size;
#ifdef SP_TEST
  std::memset(tail, 0, std::size_t(tail->size));
#endif
  head->next.store(next, std::memory_order_relaxed);
} // header::coalesce()

Free *
init_free(void *const head, sp::node_size length) noexcept {
  if (head && length > 0) {
    assert(reinterpret_cast<uintptr_t>(head) % alignof(Free) == 0);
    assert(length >= sizeof(Free));
#ifdef SP_TEST
    std::memset(head, 0, std::size_t(length));
#endif

    return new (head) Free(length, nullptr);
  }
  return nullptr;
} // header::init_free()

template <typename T>
static T *
ireduce(T *const free, sp::node_size length) noexcept {
  assert(free->size >= length);
  assert((free->size - length) >= sizeof(T));

  sp::node_size newSz(free->size - length);
  free->size = newSz;

  void *const result = util::ptr_math(free, +std::size_t(newSz));
#ifdef SP_TEST
  std::memset(result, 0, std::size_t(length));
#endif
  return new (result) T(length);
}

Free *
reduce(Free *const free, sp::node_size length) noexcept {
  return ireduce(free, length);
} // header::reduce()

Free *
free(void *const start) noexcept {
  assert(start != nullptr);
  uintptr_t startPtr = reinterpret_cast<uintptr_t>(start);
  assert(startPtr % alignof(Free) == 0);
  return reinterpret_cast<Free *>(start);
} // header::free()

/*LocalFree*/
LocalFree::LocalFree(sp::node_size sz) noexcept
    // list {{{
    : next{nullptr}
    , priv(nullptr)
    //}}}
    // tree{{{
    , left{nullptr}
    , right(nullptr)
    // }}}
    , size{sz} {
}

LocalFree::LocalFree() noexcept
    : LocalFree(sp::node_size(0)) {
}

LocalFree *
reduce(LocalFree *const free, sp::node_size length) noexcept {
  return ireduce(free, length);
} // header::reduce()

LocalFree *
init_local_free(void *const head, sp::node_size length) noexcept {
  if (head && length > 0) {
    assert(reinterpret_cast<uintptr_t>(head) % alignof(LocalFree) == 0);
    assert(length >= sizeof(LocalFree));
#ifdef SP_TEST
    std::memset(head, 0, std::size_t(length));
#endif

    return new (head) LocalFree(length);
  }
  return nullptr;
}

LocalFree *
init_local_free(void *const h, sp::node_size length, LocalFree *next) noexcept {
  auto result = init_local_free(h, length);
  if (result) {
    result->next = next;
    if (next)
      next->priv = result;
  }
  return result;
}

bool
is_consecutive(LocalFree *head, LocalFree *tail) noexcept {
  return internal_is_consecutive(head, tail);
}

/*Extent*/
static_assert(sizeof(Extent) == SP_MALLOC_CACHE_LINE_SIZE, "");
static_assert(alignof(Extent) == SP_MALLOC_CACHE_LINE_SIZE, "");

Extent::Extent() noexcept
    : reserved{false} {
}

static sp::buckets
calc_buckets(sp::node_size nodeSz, sp::bucket_size bucketSz) noexcept {
  assert(bucketSz > 0);
  assert(nodeSz > header::SIZE);

  return (nodeSz - header::SIZE) / bucketSz;
} // header::calc_buckets()

Extent *
extent(Node *const start) noexcept {
  assert(start != nullptr);
  assert(start->type == NodeType::HEAD);
  uintptr_t startPtr = reinterpret_cast<uintptr_t>(start);
  uintptr_t headerPtr = startPtr + sizeof(Node);
  assert(headerPtr % alignof(Extent) == 0);

  return reinterpret_cast<Extent *>(headerPtr);
} // header::extent()

bool
is_empty(Extent *const ext) noexcept {
  return ext->reserved.all(0);
} // header::is_empty()

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

#ifdef SP_TEST
  std::memset(raw, 0, std::size_t(size));
#endif
  Node *const nHdr = node(raw);
  new (nHdr) Node(NodeType::HEAD, size, bucketSz, buckets);

  Extent *const eHdr = extent(nHdr);
  new (eHdr) Extent;

  return nHdr;
} // header::init_node()

Node *
node(void *const start) noexcept {
  assert(start != nullptr);
  uintptr_t startPtr = reinterpret_cast<uintptr_t>(start);
  assert(startPtr % Node::ALIGNMENT == 0);
  return reinterpret_cast<Node *>(start);
} // header::node()

sp::node_size
node_data_size(Node *const node) noexcept {
  sp::node_size result = node->node_size;
  if (node->type == NodeType::HEAD) {
    assert(result >= header::SIZE);
    result = result - header::SIZE;
  } else if (node->type == NodeType::LINK) {
    assert(result >= sizeof(Node));
    result = result - sizeof(Node);
  }
  return result;
} // header::node_data_size()

std::uintptr_t
node_data_start(Node *const node) noexcept {
  uintptr_t result = reinterpret_cast<uintptr_t>(node);
  result += sizeof(Node);
  if (node->type == NodeType::HEAD) {
    result += sizeof(Extent);
  }
  return result;
} // header::node_data_start()

} // namespace header

//=======GLOBAL===============================================
namespace global {
/*State*/
State::State() noexcept //
    : brk_lock{}
    , brk_position{nullptr}
    , brk_alloc{0}
    , free(sp::node_size(0), nullptr)
#ifdef SP_TEST
    , skip_alloc(false)
#endif
{
}

} // namespace global

//=======LOCAL===============================================
namespace local {
/*Pool*/
Pool::Pool() noexcept
    : start{header::NodeType::SPECIAL, sp::node_size(0), sp::bucket_size(0),
            sp::buckets(0)}
    , lock{} {
}

/*PoolsRAII*/
PoolsRAII::PoolsRAII() noexcept
    : buckets{}
    , total_alloc{0}
    , priv{nullptr}
    , next{nullptr}
    , reclaim(false)
    // free list {{{
    , free_lock()
    , free_stack()
    , free_list(sp::node_size(0))
    , free_tree()
//}}}
{
  free_list.next = &free_list;
  free_list.priv = &free_list;
  // TODO std::atomic_thread_fence(std::memory_order_release);?
}

Pool &PoolsRAII::operator[](std::size_t idx) noexcept {
  assert(idx < BUCKETS);
  return buckets[idx];
}

/*Pools*/
Pools::Pools() noexcept
    : pools{nullptr}
    , global{nullptr} {
  // printf("ctor Pools TL\n");
}

Pools::~Pools() noexcept {
  if (pools) {
    assert(global);
    global::release_pool(*global, pools);
    pools = nullptr;
  }
  // printf("dtor Pools TL\n");
}

Pool &Pools::operator[](std::size_t idx) noexcept {
  assert(pools != nullptr);
  assert(idx < BUCKETS);
  return pools->buckets[idx];
}
void
Pools::init(global::State &state) noexcept {
  if (!pools) {
    global = &state;
    pools = global::acquire_pool(state);
  }
}

} // namespace local

//=======SHARED===============================================
namespace shared {
sp::bucket_size
bucket_size_for(std::size_t sz) noexcept {
  return sp::bucket_size{util::round_even(sz)};
}

std::size_t
pool_index(sp::bucket_size sz) noexcept {
  assert(sz % 8 == 0);
  return util::trailing_zeros(std::size_t(sz)) - std::size_t(3);
} // ::pool_index()

local::Pool &
pool_for(local::PoolsRAII &pools, sp::bucket_size sz) noexcept {
  const std::size_t index = pool_index(sz);
  return pools[index];
} // ::pool_for()

/*State*/
State::State(global::State &g, local::PoolsRAII &p,
             local::PoolsRAII &tl) noexcept
    : global(g)
    , pool(p)
    , local_pool(tl) {
}

State::State(global::State &g, local::Pools &p, local::Pools &lp) noexcept
    : State(g, *p.pools, *lp.pools) {
  assert(p.pools);
  assert(lp.pools);
}

State::State(global::State &g, local::Pools &p, local::PoolsRAII &lp) noexcept
    : State(g, *p.pools, lp) {
  assert(p.pools);
}

State::State(global::State &g, local::PoolsRAII &p, local::Pools &lp) noexcept
    : State(g, p, *lp.pools) {
  assert(lp.pools);
}

State::~State() noexcept {
}

} // namespace shared
