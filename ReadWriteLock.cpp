#include "ReadWriteLock.h"
#include <cassert>

#define SP_RW_PREPARE_MASK uint64_t(0xFF)
#define SP_RW_EXCLUSIVE_MASK uint64_t(0xFF00)
#define SP_RW_SHARED_MASK ~uint64_t(SP_RW_PREPARE_MASK | SP_RW_EXCLUSIVE_MASK)

namespace {
bool is_exclusive(std::uint64_t cmp) noexcept {
  return std::uint64_t(cmp & SP_RW_EXCLUSIVE_MASK) > 0;
}

bool is_prepare(std::uint64_t cmp) noexcept {
  return std::uint64_t(cmp & SP_RW_PREPARE_MASK) > 0;
}

bool is_shared(std::uint64_t cmp) noexcept {
  return std::uint64_t(cmp & SP_RW_SHARED_MASK) > 0;
}

uint64_t get_shared(uint64_t shared) noexcept {
  return shared >> (2 * 8);
}

uint64_t add_shared(uint64_t shared, int8_t amount) noexcept {
  shared = get_shared(shared);
  shared = shared + amount;
  shared = shared << (2 * 8);
  return shared;
}
}

/*
 * ReadWriteLock
 */
namespace sp {
ReadWriteLock::ReadWriteLock() noexcept //
    : m_state(0) {
  static_assert(sizeof(std::atomic<std::uint64_t>) == sizeof(m_state), "");
  static_assert(
      alignof(std::atomic<std::uint64_t>) == alignof(decltype(m_state)), "");
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
    return true;
  }
  return false;
}

void ReadWriteLock::shared_unlock() noexcept {
  uint64_t cmp = m_state.load(std::memory_order_acquire);
  constexpr uint64_t shared_mask = SP_RW_SHARED_MASK;
  constexpr uint64_t prep_mask = SP_RW_PREPARE_MASK;
  constexpr uint64_t ex_mask = SP_RW_EXCLUSIVE_MASK;
retry:
  // Since we should not be able to have a exclusive lock when there are
  // shared locks active, or we try to decrement a shared  lock without any
  // active.
  assert(!is_exclusive(cmp));
  assert(is_shared(cmp));
  // 1. Decrement shared
  const uint64_t shared = cmp & shared_mask;
  const uint64_t new_shared = add_shared(shared, int8_t(-1));
  // 2. Prepare unaltered
  const uint64_t prepare = cmp & prep_mask;
  // 3. Zero out exclusive bit
  const uint64_t exclusive = cmp & ex_mask;
  // 4. Build state
  const uint64_t try_unshare = new_shared | prepare | exclusive;

  if (!m_state.compare_exchange_weak(cmp, try_unshare)) {
    goto retry;
  }
}

void ReadWriteLock::eager_exclusive_lock() noexcept {
  uint64_t cmp = m_state.load(std::memory_order_acquire);
  constexpr uint64_t shared_mask = SP_RW_SHARED_MASK;
retry:
  // Shared
  const uint64_t shared = cmp & shared_mask;
  // Prepare
  const uint64_t prepare = 0x00;
  //
  cmp = shared | prepare | uint64_t(0x00);
  const uint64_t try_exclusive = cmp | uint64_t(0x01);

  if (!m_state.compare_exchange_weak(cmp, try_exclusive)) {
    goto retry;
  }

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

bool ReadWriteLock::try_exclusive_lock(bool prepare_unset,
                                       int8_t shared_dec) noexcept {
  uint64_t cmp = m_state.load(std::memory_order_acquire);
  constexpr uint64_t shared_mask = SP_RW_SHARED_MASK;
retry:
  if (prepare_unset) {
    assert(is_prepare(cmp));
  }

  if (!is_exclusive(cmp) && (!is_prepare(cmp) || prepare_unset)) {
    uint64_t shared(cmp & shared_mask);
    // assert no overflow?
    assert((get_shared(shared) - shared_dec) <= get_shared(shared));
    shared = add_shared(shared, -shared_dec);

    const uint64_t prepare(0x00);
    const uint64_t try_exclusive = shared | prepare | uint64_t(0x01);

    if (!m_state.compare_exchange_weak(cmp, try_exclusive)) {
      goto retry;
    }
    return true;
  }

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
    goto retry;
  }
}
// void ReadWriteLock::prepare_lock() noexcept {
// }

bool ReadWriteLock::try_prepare_lock(int8_t shared_dec) noexcept {
  uint64_t cmp = m_state.load(std::memory_order_acquire);
  constexpr uint64_t shared_mask = SP_RW_SHARED_MASK;
retry:
  if (!is_exclusive(cmp) && !is_prepare(cmp)) {
    uint64_t shared(cmp & shared_mask);
    // assert no overflow?
    assert((get_shared(shared) - shared_dec) <= get_shared(shared));
    shared = add_shared(shared, -shared_dec);

    const uint64_t prepare(0x00);
    const uint64_t exclusive(0x00);

    const uint64_t new_prepare(0x01 << (1 * 8));

    const uint64_t try_prepare = shared | new_prepare | exclusive;

    cmp = shared | prepare | exclusive;

    if (!m_state.compare_exchange_weak(cmp, try_prepare)) {
      goto retry;
    }
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
  m_lock->eager_exclusive_lock();
}

EagerExclusiveLock::~EagerExclusiveLock() noexcept {
  if (m_lock) {
    m_lock->exclusive_unlock();
    m_lock = nullptr;
  } else {
    assert(false);
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
    : m_lock(nullptr) {
  if (p_lock.try_exclusive_lock()) {
    m_lock = &p_lock;
  }
}

TryExclusiveLock::TryExclusiveLock(TrySharedLock &p_lock) noexcept //
    : m_lock(nullptr) {
  if (p_lock) {
    // Decrement shared and set exclusive flag
    bool prepare_unset = false;
    int8_t shared_dec = 1;
    if (p_lock.m_lock->try_exclusive_lock(prepare_unset, shared_dec)) {
      m_lock = p_lock.m_lock;
      p_lock.m_lock = nullptr;
    }
  } else {
    assert(false);
  }
}

TryExclusiveLock::TryExclusiveLock(TryPrepareLock &p_lock) noexcept //
    : m_lock(nullptr) {
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
