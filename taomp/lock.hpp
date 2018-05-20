#pragma once

#include "utils.hpp"
#include <atomic>
#include <cassert>
#include <cstdint>
#include <memory>
#include <mutex>
#include <iostream>
#include <chrono>

namespace taomp {

class NoBackoff {
public:
  void backoff() const {}
};

class ExpBackoff {
  using ClockTy = std::chrono::high_resolution_clock;
  using DurationTy = std::chrono::nanoseconds;
  DurationTy min, max;
  mutable DurationTy state;
  static ClockTy::time_point now() {
    return ClockTy::now();
  }
public:
  ExpBackoff(DurationTy min, DurationTy max) : min(min), max(max), state(min) {}
  void backoff() const {
    ClockTy::time_point end = now() + state;
    state *= 2;
    if (state > max) {
      state = min;
    }
    while (now() < end) {
      continue;
    }
  }
};

/**TASLock satisfies the following concept:
 * BasicLockable/Lockable
 * Besides, this class is NOT:
 * Copyable, Moveable
 */
class TASLock {
protected:
  std::atomic<bool> state{false};

public:
  TASLock() = default;
  TASLock(const TASLock &) = delete;
  TASLock(TASLock &&) = delete;
  TASLock &operator=(const TASLock &) = delete;
  TASLock &operator=(TASLock &&) = delete;

  template <class T>
  void lock(const T & backoffer) {
    while (state.exchange(true, std::memory_order_acq_rel)) {
      backoffer.backoff();
    }
  }

  void lock() {
    return lock(NoBackoff());
  }

  void unlock() { state.store(false, std::memory_order_release); }

  bool try_lock() { return !state.exchange(true, std::memory_order_acq_rel); }
};

class TTASLock : public TASLock {
public:
  TTASLock() = default;
  template <class T>
  void lock(const T & backoffer) {
    while (state.exchange(true, std::memory_order_acq_rel)) {
      while (state.load(std::memory_order_relaxed)) {
        backoffer.backoff();
      }
    }
  }
  void lock() {
    return lock(NoBackoff());
  }
};

/***ArrayLock doesn't satisfy Lockable concept(no try_lock())
 */
class ArrayLock {
  std::atomic<bool> *ptr;
  std::atomic<unsigned> pos;
  unsigned bucket_mask;

public:
  using HandleT = unsigned;
  ArrayLock(unsigned thread_num)
      : ptr(nullptr), pos(0), bucket_mask(MaskLeadingZero(thread_num)) {
    assert(thread_num);
    ptr = new std::atomic<bool>[thread_num << 4];
    assert(ptr);
    for (int i = 1; i < thread_num; ++i) {
      ptr[i] = false;
    }
    ptr[0] = true;
  };

  HandleT lock() {
    unsigned handle = pos.fetch_add(1, std::memory_order_acq_rel);
    handle &= bucket_mask;
    while (!ptr[handle << 4].load(std::memory_order_acquire)) {
      continue;
    }
    return handle;
  }

  void unlock(HandleT handle) {
    ptr[handle << 4].store(false, std::memory_order_relaxed);
    handle = (handle + 1) & bucket_mask;
    ptr[handle << 4].store(true, std::memory_order_release);
  }
};

template <typename T> class HandleLockGuard {
  T &lock;
  typename T::HandleT handle;

public:
  HandleLockGuard(T &lock) : lock(lock), handle(lock.lock()) {}
  ~HandleLockGuard() { lock.unlock(handle); }
  HandleLockGuard &operator=(HandleLockGuard &) = delete;
};

class CLHLock {
public:
  using QNode = std::atomic<bool>;
  CLHLock(BORROW(QNode *init_tail = nullptr)) : tail(init_tail) {}

  GIVE(QNode *) lock(TAKE(QNode *in)) {
    in->store(true, std::memory_order_relaxed);
    QNode *ret = tail.exchange(in, std::memory_order_acq_rel);
    while (ret->load(std::memory_order_acquire)) {
      while (ret->load(std::memory_order_relaxed)) {
        continue;
      }
    }
    return ret;
  }

  void unlock(BORROW(QNode *in)) {
    in->store(false, std::memory_order_release);
  }

  void reset(BORROW(QNode *init_tail)) {
    tail.store(init_tail, std::memory_order_relaxed);
  }

private:
  std::atomic<QNode *> tail;
};

class MCSLock {
public:
  using QNode = std::atomic<uintptr_t>;
  MCSLock(BORROW(QNode *init_tail = nullptr)) : tail(init_tail) {}
  void lock(BORROW(QNode *in)) {
    QNode *old_tail = tail.exchange(in, std::memory_order_acq_rel);
    if (old_tail) {
      old_tail->fetch_add(reinterpret_cast<uintptr_t>(in), std::memory_order_release);
      uintptr_t v;
      do {
        v = in->load(std::memory_order_acquire) & 1;
      } while (!v);
    } else {
      in->fetch_add(1, std::memory_order_release);
    }
  }
  void unlock(BORROW(QNode *in)) {
    QNode* in1 = in;
    if (tail.compare_exchange_strong(in1, nullptr, std::memory_order_acq_rel,
                              std::memory_order_acquire)) {
      in->store(0, std::memory_order_release);
      return;
    }
    uintptr_t v = in->load(std::memory_order_acquire);
    do {
      v = in->load(std::memory_order_acquire) ^ 1;
    } while (!v);
    reinterpret_cast<std::atomic<uintptr_t> *>(v)->fetch_add(
        1, std::memory_order_release);
    in->store(0, std::memory_order_release);
  }

private:
  std::atomic<QNode *> tail;
};

} // namespace taomp

namespace std {
template <>
class lock_guard<taomp::ArrayLock>
    : public taomp::HandleLockGuard<taomp::ArrayLock> {
  using BaseT = taomp::HandleLockGuard<taomp::ArrayLock>;

public:
  using BaseT::BaseT;
};
} // namespace std

