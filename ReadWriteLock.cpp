#include "ReadWriteLock.h"
#include <cassert>

namespace {
bool is_exclusive(std::uint64_t cmp) noexcept {
  return cmp == std::uint64_t(cmp & ~std::uint64_t(0x01));
}
}

/*
 * ReadWriteLock
 */
namespace sp {
ReadWriteLock::ReadWriteLock() //
    : m_state(0) {
  static_assert(sizeof(std::atomic<std::uint64_t>) == sizeof(m_state), "");
  static_assert(
      alignof(std::atomic<std::uint64_t>) == alignof(decltype(m_state)), "");
}

ReadWriteLock::~ReadWriteLock() {
}

void ReadWriteLock::shared_lock() noexcept {
  uint64_t cmp = m_state.load(std::memory_order_acquire);
  constexpr uint64_t mask = ~uint64_t(0xff);
retry:
  cmp = cmp & mask;
  uint64_t shared = cmp & (~mask);
  shared = shared >> 1;
  shared = shared + 1;
  shared = shared << 1;
  uint64_t value = shared | 0x00;

  if (!m_state.compare_exchange_weak(cmp, value)) {
    goto retry;
  }
}

void ReadWriteLock::shared_unlock() noexcept {
  uint64_t cmp = m_state.load(std::memory_order_acquire);
  constexpr uint64_t mask = ~uint64_t(0xff);
retry:
  uint64_t exclusive = cmp & mask;
  uint64_t shared = cmp & (~mask);
  shared = shared >> 1;
  shared = shared - 1;
  shared = shared << 1;

  uint64_t value = shared | exclusive;

  if (!m_state.compare_exchange_weak(cmp, value)) {
    goto retry;
  }
}

void ReadWriteLock::eager_exclusive_lock() noexcept {
  uint64_t cmp = m_state.load(std::memory_order_acquire);
  constexpr uint64_t mask = ~uint64_t(0xff);
retry:
  cmp = cmp & (~mask);
  uint64_t value = cmp | 0x01;

  if (!m_state.compare_exchange_weak(cmp, value)) {
    goto retry;
  }
  while (m_state.load() != 0x01)
    ;
}

void ReadWriteLock::lazy_exclusive_lock() noexcept {
retry:
  uint64_t cmp(0);
  uint64_t value = uint64_t(0x00) | uint64_t(0x01);

  if (!m_state.compare_exchange_weak(cmp, value)) {
    goto retry;
  }
}

bool ReadWriteLock::try_exclusive_lock() noexcept {
  uint64_t cmp = m_state.load(std::memory_order_acquire);
retry:
  if (!is_exclusive(cmp)) {
    uint64_t value = cmp | 0x01;
    if (!m_state.compare_exchange_weak(cmp, value)) {
      goto retry;
    }
    return true;
  }

  return false;
}

void ReadWriteLock::exclusive_unlock() noexcept {
  uint64_t cmp = m_state.load(std::memory_order_acquire);
retry:
  uint64_t value = cmp & (~0x01);

  if (!m_state.compare_exchange_weak(cmp, value)) {
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

SharedLock::~SharedLock() {
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
    : m_lock(&p_lock) {
  // TODO
}

TrySharedLock::~TrySharedLock() {
  // TODO
}

TrySharedLock::operator bool() const noexcept {
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

EagerExclusiveLock::~EagerExclusiveLock() {
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

LazyExclusiveLock::~LazyExclusiveLock() {
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

TryExclusiveLock::TryExclusiveLock(SharedLock &) noexcept //
    : m_lock(nullptr) {
  // TODO
}
TryExclusiveLock::TryExclusiveLock(TrySharedLock &) noexcept //
    : m_lock(nullptr) {
  // TODO
}

TryExclusiveLock::~TryExclusiveLock() {
  if (m_lock) {
    m_lock->exclusive_unlock();
    m_lock = nullptr;
  }
}

TryExclusiveLock::operator bool() const noexcept {
  return m_lock != nullptr;
}

} // namespace sp
