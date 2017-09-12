#include "ReadWriteLock.h"
#include <cassert>
#include <stdio.h>
#include <utility>

#define SP_RW_PREPARE_MASK uint64_t(0xFF00)
#define SP_RW_EXCLUSIVE_MASK uint64_t(0xFF)
#define SP_RW_SHARED_MASK ~uint64_t(SP_RW_PREPARE_MASK | SP_RW_EXCLUSIVE_MASK)

// #define SP_DEBUG_RW
#ifdef SP_DEBUG_RW
#include <algorithm>
#include <stdio.h>
#include <string.h>
template <typename T, std::size_t SIZE>
static void internal_print_state(T state, char (&hexed)[SIZE]) {
  char *arr = (char *)&state;
  const std::size_t hex_cap = SIZE - 1;
  memset(hexed, 0, SIZE);

  std::size_t hex_length = 0;
  std::size_t i = 0;
  while (i < sizeof(state) && hex_length < hex_cap) {
    char buff[128];
    std::size_t buffLength = sprintf(buff, "%02x", arr[i++]);
    memcpy(hexed + hex_length, buff, buffLength);

    hex_length += buffLength;
  }
}
#endif

namespace {

static uint8_t get_exclusive(std::uint64_t state) noexcept {
  return uint8_t(state & 0xFF);
}

bool is_exclusive(std::uint64_t cmp) noexcept {
  return get_exclusive(cmp) > 0;
}

static uint8_t get_prepare(std::uint64_t state) noexcept {
  return uint8_t((state >> 8) & 0xFF);
}

bool is_prepare(std::uint64_t cmp) noexcept {
  return get_prepare(cmp) > 0;
}

uint64_t get_shared(uint64_t shared) noexcept {
  return shared >> (2 * 8);
}

bool has_shared(std::uint64_t cmp) noexcept {
  return get_shared(cmp) > 0;
}

uint64_t add_shared(uint64_t shared, int8_t amount) noexcept {
  shared = get_shared(shared);
  shared = shared + amount;
  shared = shared << (2 * 8);
  return shared;
}
} // namespace
static void print_state(const char *ctx, const uint64_t state) {
#ifdef SP_DEBUG_RW
  char cshared[sizeof(uint64_t) * 4];
  memset(cshared, 0, sizeof(cshared));
  char cprepare[sizeof(uint64_t) * 4];
  memset(cprepare, 0, sizeof(cprepare));
  char cex[sizeof(uint64_t) * 4];
  memset(cex, 0, sizeof(cex));

  internal_print_state(get_shared(state), cshared);
  internal_print_state(get_prepare(state), cprepare);
  internal_print_state(get_exclusive(state), cex);

  char context[50];
  memset(context, ' ', sizeof(context));
  context[sizeof(context) - 1] = 0;

  size_t ctx_len = std::max(int64_t(0), int64_t(strlen(ctx)));
  size_t length = std::min(sizeof(context), ctx_len);
  memcpy(context, ctx, length);

  // uintptr_t p = reinterpret_cast<uintptr_t>(lock);
  // printf("s%lu,%d,%d",get_shared(state))
  printf("%s"
         // "[%p]:"
         "s[\e[95m%s\e[0m]"
         "pre[\e[92m%s\e[0m]"
         "ex[\e[93m%s\e[0m]\n",
         context, //
         // #<{(|C[p % sizeof(C)],|)}># p, //
         cshared, cprepare, cex);
#endif
}

/*
 * ReadWriteLock
 */
namespace sp {
ReadWriteLock::ReadWriteLock() noexcept //
    : m_state(0) {
  using TYPE = std::atomic<std::uint64_t>;
  static_assert(sizeof(TYPE) == sizeof(m_state), "");
  static_assert(alignof(TYPE) == alignof(decltype(m_state)), "");
}

ReadWriteLock::~ReadWriteLock() noexcept {
}

void ReadWriteLock::shared_lock() noexcept {
retry:
  if (!try_shared_lock()) {
    goto retry;
  }
}

bool ReadWriteLock::try_shared_lock() noexcept {
  uint64_t cmp = m_state.load(std::memory_order_acquire);
  constexpr uint64_t shared_mask = SP_RW_SHARED_MASK;
  constexpr uint64_t prep_mask = SP_RW_PREPARE_MASK;
retry:
  if (!is_exclusive(cmp)) {
    const uint64_t shared = cmp & shared_mask;
    const uint64_t new_shared = add_shared(shared, int8_t(1));

    const uint64_t prepare(cmp & prep_mask);
    const uint64_t exclusive(0x00);

    // cmp = shared | prepare | exclusive;
    const uint64_t try_share = new_shared | prepare | exclusive;

    if (!m_state.compare_exchange_weak(cmp, try_share)) {
      goto retry;
    }
    print_state("\e[41m \e[0mtry_shared_lock_before", cmp);
    print_state("\e[41m \e[0mtry_shared_lock_after[sucess]", try_share);
    return true;
  }
  print_state("\e[41m \e[0mtry_shared_lock_after[failed]", cmp);
  return false;
}

void ReadWriteLock::shared_unlock() noexcept {
  uint64_t cmp = m_state.load(std::memory_order_acquire);
  constexpr uint64_t shared_mask = SP_RW_SHARED_MASK;
  constexpr uint64_t prep_mask = SP_RW_PREPARE_MASK;
  constexpr uint64_t ex_mask = SP_RW_EXCLUSIVE_MASK;
retry:
  assert(has_shared(cmp));
  // 1. Decrement shared
  const uint64_t shared = cmp & shared_mask;
  const uint64_t new_shared = add_shared(shared, -int8_t(1));
  // 2. Prepare unaltered
  const uint64_t prepare = cmp & prep_mask;
  // 3. Zero out exclusive bit
  const uint64_t exclusive = cmp & ex_mask;
  // 4. Build state
  const uint64_t try_unshare = new_shared | prepare | exclusive;

  if (!m_state.compare_exchange_weak(cmp, try_unshare)) {
    print_state("\e[41m \e[0mtry_shared_unlock_retry", cmp);
    goto retry;
  }
  print_state("\e[41m \e[0mtry_shared_unlock_before", cmp);
  print_state("\e[41m \e[0mtry_shared_unlock_after", try_unshare);
}

void ReadWriteLock::eager_exclusive_lock(bool decShared,
                                         bool unsetPrepare) noexcept {
  uint64_t cmp = m_state.load(std::memory_order_acquire);
  constexpr uint64_t shared_mask = SP_RW_SHARED_MASK;
retry:
  if (unsetPrepare) {
    assert(is_prepare(cmp));
  }

  if (decShared) {
    assert(has_shared(cmp));
  }
  // Shared
  const uint64_t shared = cmp & shared_mask;
  // Prepare
  const uint64_t prepare = unsetPrepare ? (0x01 << (1 * 8)) : 0x00;
  //
  cmp = shared | prepare | uint64_t(0x00);

  uint64_t new_shared = decShared ? add_shared(shared, int8_t(-1)) : shared;
  uint64_t try_exclusive = new_shared | uint64_t(0x00) | uint64_t(0x01);

  if (!m_state.compare_exchange_weak(cmp, try_exclusive)) {
  print_state("\e[44m \e[0meager_exclusive_lock_retry", cmp);
    goto retry;
  }
  print_state("\e[44m \e[0meager_exclusive_lock_before", cmp);
  print_state("\e[44m \e[0meager_exclusive_lock_after", try_exclusive);

  while (m_state.load(std::memory_order_acquire) != uint64_t(0x01))
    ;
}

void ReadWriteLock::lazy_exclusive_lock() noexcept {
  const uint64_t shared(0x00);
  const uint64_t prepare(0x00);
  const uint64_t exclusive(0x00);
retry:
  uint64_t cmp = shared | prepare | exclusive;

  const uint64_t try_exclusive = shared | prepare | uint64_t(0x01);

  if (!m_state.compare_exchange_weak(cmp, try_exclusive)) {
    goto retry;
  }
}

bool ReadWriteLock::try_exclusive_lock(bool preUnset, int8_t shaDec) noexcept {
  uint64_t cmp = m_state.load(std::memory_order_acquire);
  constexpr uint64_t shared_mask = SP_RW_SHARED_MASK;
  constexpr uint64_t prep_mask = SP_RW_PREPARE_MASK;
retry:
  if (preUnset) {
    assert(is_prepare(cmp));
  }

  const uint64_t shared(cmp & shared_mask);
  // assert no overflow?
  assert((get_shared(shared) - shaDec) <= get_shared(shared));
  const uint64_t new_shared = add_shared(shared, -shaDec);

  const uint64_t prepare(cmp & prep_mask);
  const uint64_t new_prepare = preUnset ? 0x00 : prepare;

  bool hasShared = has_shared(new_shared);
  bool isExclusive = is_exclusive(cmp);
  bool isPrepare = is_prepare(new_prepare);
  if (!hasShared && !isExclusive && !isPrepare) {

    cmp = shared | prepare | uint64_t(0x00);
    const uint64_t try_exclusive = new_shared | new_prepare | uint64_t(0x01);

    if (!m_state.compare_exchange_weak(cmp, try_exclusive)) {
      goto retry;
    }
    print_state("try_exclusive_lock_before", cmp);
    print_state("try_exclusive_lock_after[success]", try_exclusive);
    return true;
  }
  print_state("try_exclusive_lock_after", cmp);

  return false;
}

void ReadWriteLock::exclusive_unlock() noexcept {
  uint64_t cmp = m_state.load(std::memory_order_acquire);
  constexpr uint64_t shared_mask = SP_RW_SHARED_MASK;
  constexpr uint64_t prep_mask = SP_RW_PREPARE_MASK;
retry:
  assert(is_exclusive(cmp));
  const uint64_t shared(cmp & shared_mask);
  const uint64_t prepare(cmp & prep_mask);
  const uint64_t try_unexclusive = shared | prepare | uint64_t(0x00);

  if (!m_state.compare_exchange_weak(cmp, try_unexclusive)) {
    print_state("\e[44m \e[0meager_exclusive_unlock_retry", cmp);
    goto retry;
  }
  print_state("\e[44m \e[0meager_exclusive_unlock_before", cmp);
  print_state("\e[44m \e[0meager_exclusive_unlock_after", try_unexclusive);
}
// void ReadWriteLock::prepare_lock() noexcept {
// }

bool ReadWriteLock::try_prepare_lock(int8_t shared_dec) noexcept {
  uint64_t cmp = m_state.load(std::memory_order_acquire);
  constexpr uint64_t shared_mask = SP_RW_SHARED_MASK;
retry:
  if (!is_exclusive(cmp) && !is_prepare(cmp)) {
    const uint64_t shared(cmp & shared_mask);
    // assert no overflow?
    assert((get_shared(shared) - shared_dec) <= get_shared(shared));
    const uint64_t new_shared = add_shared(shared, -shared_dec);

    const uint64_t prepare(0x00);
    const uint64_t exclusive(0x00);

    const uint64_t new_prepare(0x01 << (1 * 8));
    const uint64_t try_prepare = new_shared | new_prepare | exclusive;

    cmp = shared | prepare | exclusive;

    if (!m_state.compare_exchange_weak(cmp, try_prepare)) {
      goto retry;
    }
    print_state("\e[42m \e[0mtry_prepare_lock_before", cmp);
    print_state("\e[42m \e[0mtry_prepare_lock_after[success]", try_prepare);
    return true;
  }
  return false;
}
void ReadWriteLock::prepare_unlock() noexcept {
  uint64_t cmp = m_state.load(std::memory_order_acquire);
  constexpr uint64_t shared_mask = SP_RW_SHARED_MASK;
  constexpr uint64_t prep_mask = SP_RW_PREPARE_MASK;
  constexpr uint64_t ex_mask = SP_RW_EXCLUSIVE_MASK;
retry:
  assert(is_prepare(cmp));
  assert(!is_exclusive(cmp));
  const uint64_t shared(cmp & shared_mask);
  const uint64_t prepare(cmp & prep_mask);
  const uint64_t exclusive(cmp & ex_mask);

  cmp = shared | prepare | exclusive;

  const uint64_t try_unprepare = shared | uint64_t(0x00) | exclusive;

  if (!m_state.compare_exchange_weak(cmp, try_unprepare)) {
    goto retry;
  }
  print_state("try_prepare_unlock_before", cmp);
  print_state("try_prepare_unlock_after", try_unprepare);
}

uint64_t ReadWriteLock::shared_locks() const noexcept {
  return get_shared(m_state.load(std::memory_order_acquire));
}

bool ReadWriteLock::has_prepare_lock() const noexcept {
  return is_prepare(m_state.load(std::memory_order_acquire));
}

bool ReadWriteLock::has_exclusive_lock() const noexcept {
  return is_exclusive(m_state.load(std::memory_order_acquire));
}

} // namespace sp

/*
 * SharedLock
 */
namespace sp {
SharedLock::SharedLock(ReadWriteLock &p_lock) noexcept //
    : m_lock(&p_lock) {
  m_lock->shared_lock();
}

SharedLock::~SharedLock() noexcept {
  if (m_lock) {
    m_lock->shared_unlock();
    m_lock = nullptr;
  } else {
    assert(false);
  }
}

SharedLock::operator bool() const noexcept {
  return m_lock != nullptr;
}
} // namespace sp

/*
 * TrySharedLock
 */
namespace sp {
TrySharedLock::TrySharedLock(ReadWriteLock &p_lock) noexcept //
    : m_lock(nullptr) {
  if (p_lock.try_shared_lock()) {
    m_lock = &p_lock;
  }
}

TrySharedLock::~TrySharedLock() noexcept {
  if (m_lock) {
    m_lock->shared_unlock();
    m_lock = nullptr;
  }
}

TrySharedLock::operator bool() const noexcept {
  return m_lock != nullptr;
}

void TrySharedLock::swap(TrySharedLock &o) noexcept {
  std::swap(m_lock, o.m_lock);
}

} // namespace sp

/*
 * TryPrepareLock
 */
namespace sp {
TryPrepareLock::TryPrepareLock(ReadWriteLock &p_lock) noexcept //
    : m_lock(nullptr) {
  if (p_lock.try_prepare_lock()) {
    m_lock = &p_lock;
  }
}

TryPrepareLock::TryPrepareLock(TrySharedLock &p_lock) noexcept //
    : m_lock(nullptr) {
  if (p_lock) {
    // Decrement shared count and set prepare flag
    int8_t shared_dec = 1;
    if (p_lock.m_lock->try_prepare_lock(shared_dec)) {
      m_lock = p_lock.m_lock;
      p_lock.m_lock = nullptr;
    }
  } else {
    assert(false);
  }
}

TryPrepareLock::~TryPrepareLock() noexcept {
  if (m_lock) {
    m_lock->prepare_unlock();
    m_lock = nullptr;
  }
}

TryPrepareLock::operator bool() const noexcept {
  return m_lock != nullptr;
}
} // namespace sp
/*
 * EagerExclusiveLock
 */
namespace sp {
EagerExclusiveLock::EagerExclusiveLock(ReadWriteLock &p_lock) noexcept //
    : m_lock(&p_lock) {
  m_lock->eager_exclusive_lock(false, false);
}

EagerExclusiveLock::EagerExclusiveLock(TryPrepareLock &p_lock) noexcept //
    : m_lock(nullptr) {
  if (p_lock) {
    p_lock.m_lock->eager_exclusive_lock(false, true);
    m_lock = p_lock.m_lock;
    p_lock.m_lock = nullptr;
  }
}

EagerExclusiveLock::~EagerExclusiveLock() noexcept {
  if (m_lock) {
    m_lock->exclusive_unlock();
    m_lock = nullptr;
  } else {
    printf("no_ex_lock\n");
  }
}

EagerExclusiveLock::operator bool() const noexcept {
  return m_lock != nullptr;
}

} // namespace sp

/*
 * LazyExclusiveLock
 */
namespace sp {
LazyExclusiveLock::LazyExclusiveLock(ReadWriteLock &p_lock) noexcept //
    : m_lock(&p_lock) {
  m_lock->lazy_exclusive_lock();
}

LazyExclusiveLock::~LazyExclusiveLock() noexcept {
  if (m_lock) {
    m_lock->exclusive_unlock();
    m_lock = nullptr;
  } else {
    assert(false);
  }
}

LazyExclusiveLock::operator bool() const noexcept {
  return m_lock != nullptr;
}

} // namespace sp

/*
 * TryExclusiveLock
 */
namespace sp {
TryExclusiveLock::TryExclusiveLock(ReadWriteLock &p_lock) noexcept //
    : m_lock{nullptr} {
  if (p_lock.try_exclusive_lock()) {
    m_lock = &p_lock;
  }
}

// TryExclusiveLock::TryExclusiveLock(TrySharedLock &p_lock) noexcept //
//     : m_lock{nullptr} {
//   print_state("try_exclusive_lock_before[TS]", p_lock.m_lock);
//   if (p_lock) {
//     print_state("try_exclusive_lock_after[success][TS]", p_lock.m_lock);
//     // Decrement shared and set exclusive flag
//     bool prepare_unset = false;
//     int8_t shared_dec = 1;
//     if (p_lock.m_lock->try_exclusive_lock(prepare_unset, shared_dec)) {
//       m_lock = p_lock.m_lock;
//       p_lock.m_lock = nullptr;
//     }
//   } else {
//     assert(false);
//   }
// }

TryExclusiveLock::TryExclusiveLock(TryPrepareLock &p_lock) noexcept //
    : m_lock{nullptr} {
  if (p_lock) {
    // Unset prepare flag and set exclusive flag
    bool prepare_unset = true;
    if (p_lock.m_lock->try_exclusive_lock(prepare_unset)) {
      m_lock = p_lock.m_lock;
      p_lock.m_lock = nullptr;
    }
  } else {
    assert(false);
  }
}

TryExclusiveLock::~TryExclusiveLock() noexcept {
  if (m_lock) {
    m_lock->exclusive_unlock();
    m_lock = nullptr;
  }
}

TryExclusiveLock::operator bool() const noexcept {
  return m_lock != nullptr;
}

} // namespace sp
