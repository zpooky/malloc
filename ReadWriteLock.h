#ifndef _SP_MALLOC_H
#define _SP_MALLOC_H

#include <atomic>
#include <cstdint>

// TODO tryLock
namespace sp {
class ReadWriteLock { //
private:
  std::atomic<std::uint64_t> m_state;

public:
  ReadWriteLock();
  ~ReadWriteLock();

public:
  // Shared lock increment shared count if exclusive bit is 0
  void shared_lock() noexcept;
  void shared_unlock() noexcept;

public:
  // Eager exclusive lock swap exclusive bit to 1 and wait until shared is 0
  void eager_exclusive_lock() noexcept;

public:
  // Lazy exclusive lock swap exclusive bit to 1 if shared is 0 otherwise retry
  void lazy_exclusive_lock() noexcept;

public:
  bool try_exclusive_lock() noexcept;

public:
  void exclusive_unlock() noexcept;
};
/*
 * SharedLock
 */
class SharedLock { //
private:
  ReadWriteLock *m_lock;

public:
  explicit SharedLock(ReadWriteLock &);
  ~SharedLock();

  operator bool() const;
};

/*
 * TrySharedLock
 */
class TrySharedLock { //
private:
  ReadWriteLock *m_lock;

public:
  explicit TrySharedLock(ReadWriteLock &);
  ~TrySharedLock();

  operator bool() const;
};


/*
 * EagerExclusiveLock
 */
class EagerExclusiveLock { //
private:
  ReadWriteLock *m_lock;

public:
  explicit EagerExclusiveLock(ReadWriteLock &);
  ~EagerExclusiveLock();

  operator bool() const;
};

/*
 * LazyExclusiveLock
 */
class LazyExclusiveLock { //
private:
  ReadWriteLock *m_lock;

public:
  explicit LazyExclusiveLock(ReadWriteLock &);
  ~LazyExclusiveLock();

  operator bool() const;
};

/*
 * TryExclusiveLock
 */
class TryExclusiveLock { //
private:
  ReadWriteLock *m_lock;

public:
  TryExclusiveLock(ReadWriteLock &);
  TryExclusiveLock(SharedLock&);
  ~TryExclusiveLock();


  operator bool() const;
};

}

#endif
