#pragma once

#include "pthread.h"
#include <cassert>

namespace taomp {
/**PThreadSpinLock provides a wrapper for pthead_spinlock_t, whatever its
 * implementation is. This class satisfies the following concepts:
 * BasicLockable/Lockable
 * Besides, this class is NOT:
 * Copyable, Moveable
 */
class PThreadSpinLock {
  pthread_spinlock_t spinlock;
public:
  PThreadSpinLock() {
    bool b = init();
    assert(b);
  }
  ~PThreadSpinLock() {
    bool b = destroy();
    assert(b);
  }

  PThreadSpinLock(const PThreadSpinLock&) = delete;
  PThreadSpinLock(PThreadSpinLock&&) = delete;
  PThreadSpinLock& operator=(const PThreadSpinLock&) = delete;
  PThreadSpinLock& operator=(PThreadSpinLock&&) = delete;

  bool init() {
    return !pthread_spin_init(&spinlock, PTHREAD_PROCESS_PRIVATE);
  }

  bool destroy() {
    return !pthread_spin_destroy(&spinlock);
  }

  void lock() {
    auto ret = pthread_spin_lock(&spinlock);
    assert(!ret);
  }

  void unlock() {
    auto ret = pthread_spin_unlock(&spinlock);
    assert(!ret);
  }

  bool try_lock() {
    return !pthread_spin_trylock(&spinlock);
  }
};
}
