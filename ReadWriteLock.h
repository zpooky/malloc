#ifndef _SP_MALLOC_H
#define _SP_MALLOC_H

#include <atomic>
#include <cstdint>

namespace sp {
class ReadWriteLock { //
private:
  std::atomic<std::uint64_t> m_state;

public:
  ReadWriteLock();
  ~ReadWriteLock();

public:
  void shared_lock() noexcept;
  void shared_unlock() noexcept;

public:
  void exclusive_lock() noexcept;
  void exclusive_unlock() noexcept;
};

class ExclusiveLock { //
private:
  ReadWriteLock &m_lock;

public:
  explicit ExclusiveLock(ReadWriteLock &);
  ~ExclusiveLock();
};

class SharedLock { //
private:
  ReadWriteLock &m_lock;

public:
  explicit SharedLock(ReadWriteLock &);
  ~SharedLock();
};
}

#endif
