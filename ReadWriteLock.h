#ifndef _SP_MALLOC_H
#define _SP_MALLOC_H

#include <atomic>
#include <cstdint>

namespace sp {
class ReadWriteLock { //
private:
public:
  // TODO encapsulation
  std::atomic<std::uint64_t> m_state;

  ReadWriteLock() noexcept;
  ~ReadWriteLock() noexcept;

public:
  // Shared lock increment shared count if exclusive bit is 0
  void
  shared_lock() noexcept;
  bool
  try_shared_lock() noexcept;
  void
  shared_unlock() noexcept;

public:
  // Eager exclusive lock swap exclusive bit to 1 and wait until shared is 0
  void
  eager_exclusive_lock(bool decShared = false,
                       bool unsetPrepare = false) noexcept;
  // Lazy exclusive lock swap exclusive bit to 1 if shared is 0 otherwise retry
  void
  lazy_exclusive_lock() noexcept;
  bool
  try_exclusive_lock(bool prepare_unset = false,
                     int8_t shared_dec = 0) noexcept;
  void
  exclusive_unlock() noexcept;

public:
  void
  prepare_lock(int8_t shared_dec = 0) noexcept;

  bool
  try_prepare_lock(int8_t shared_dec = 0) noexcept;
  void
  prepare_unlock() noexcept;

public:
  uint64_t
  shared_locks() const noexcept;
  bool
  has_prepare_lock() const noexcept;
  bool
  has_exclusive_lock() const noexcept;
};
/*
 * SharedLock
 */
class SharedLock { //
public:
  ReadWriteLock *m_lock;

public:
  explicit SharedLock(ReadWriteLock &) noexcept;
  ~SharedLock() noexcept;

  explicit operator bool() const noexcept;
};

/*
 * TrySharedLock
 */
class TrySharedLock { //
  // TODO encapsulation
public:
  ReadWriteLock *m_lock;

public:
  explicit TrySharedLock(ReadWriteLock &) noexcept;
  ~TrySharedLock() noexcept;

  explicit operator bool() const noexcept;
  void
  swap(TrySharedLock &) noexcept;
};

/*
 * PrepareLock
 */
class PrepareLock {
  // TODO encapsulation
public:
  ReadWriteLock *m_lock;

public:
  explicit PrepareLock(ReadWriteLock &) noexcept;
  explicit PrepareLock(TrySharedLock &) noexcept;
  explicit PrepareLock(SharedLock &) noexcept;
  ~PrepareLock() noexcept;
  explicit operator bool() const noexcept;
};

/*
 * TryPrepareLock
 */
class TryPrepareLock {
  // TODO encapsulation
public:
  ReadWriteLock *m_lock;

public:
  explicit TryPrepareLock(ReadWriteLock &) noexcept;
  explicit TryPrepareLock(TrySharedLock &) noexcept;
  explicit TryPrepareLock(SharedLock &) noexcept;

  ~TryPrepareLock() noexcept;
  explicit operator bool() const noexcept;
};

/*
 * EagerExclusiveLock
 */
class EagerExclusiveLock { //
private:
  ReadWriteLock *m_lock;

public:
  explicit EagerExclusiveLock(ReadWriteLock &) noexcept;
  explicit EagerExclusiveLock(TryPrepareLock &) noexcept;
  ~EagerExclusiveLock() noexcept;

  explicit operator bool() const noexcept;
};

/*
 * LazyExclusiveLock
 */
class LazyExclusiveLock { //
private:
  ReadWriteLock *m_lock;

public:
  explicit LazyExclusiveLock(ReadWriteLock &) noexcept;
  ~LazyExclusiveLock();

  explicit operator bool() const noexcept;
};

/*
 * TryExclusiveLock
 */
class TryExclusiveLock { //
private:
  ReadWriteLock *m_lock;

public:
  explicit TryExclusiveLock(ReadWriteLock &) noexcept;
  // explicit TryExclusiveLock(TrySharedLock &) noexcept;
  explicit TryExclusiveLock(TryPrepareLock &) noexcept;
  ~TryExclusiveLock() noexcept;

  explicit operator bool() const noexcept;
};
}

#endif
