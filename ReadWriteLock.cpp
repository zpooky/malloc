#include "ReadWriteLock.h"

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
  uint64_t value(cmp + 1);
  if (!m_state.compare_exchange_weak(cmp, value)) {
    goto retry;
  }
}

void ReadWriteLock::shared_unlock() noexcept {
  uint64_t cmp = m_state.load(std::memory_order_acquire);
retry:
  uint64_t exclusive = cmp & 0xff;
  uint64_t shared = cmp & (~0xff);
  shared = shared - 1;
  uint64_t value = shared | exclusive;
  if (!m_state.compare_exchange_weak(cmp, value)) {
    goto retry;
  }
}

void ReadWriteLock::exclusive_lock() noexcept {
  uint64_t cmp = m_state.load(std::memory_order_acquire);
retry:
  uint64_t exclusive = cmp & (~0xff);
  uint64_t value = exclusive | 0x01;
  if (!m_state.compare_exchange_weak(exclusive, value)) {
    goto retry;
  }
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
 * ExclusiveLock
 */
namespace sp {
ExclusiveLock::ExclusiveLock(ReadWriteLock &p_lock) //
    : m_lock(p_lock) {
  m_lock.exclusive_lock();
}

ExclusiveLock::~ExclusiveLock() {
  m_lock.exclusive_unlock();
}

} // namespace sp

/*
 * SharedLock
 */
namespace sp {
SharedLock::SharedLock(ReadWriteLock &p_lock) //
    : m_lock(p_lock) {
  m_lock.shared_lock();
}

SharedLock::~SharedLock() {
  m_lock.shared_unlock();
}

} // namespace sp
