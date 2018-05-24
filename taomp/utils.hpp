#pragma once

#include <atomic>
#include <cassert>
#include <climits>
#include <cstdint>
#include <new>
#include <stdlib.h>
#include <type_traits>
#include <utility>

// when a input argument is marked as TAKE, you don't have ownership of the
// argument after calling the function
#define TAKE(x) x
// when a input argument is marker as BORROW, calling the function has no effect
// on the ownership of that argument
#define BORROW(x) x
// when a return value is marked as GIVE, you are given the ownership of the
// return value after calling the function
#define GIVE(x) x

namespace std {
inline constexpr std::size_t hardware_destructive_interference_size = 64;
inline constexpr std::size_t hardware_constructive_interference_size = 64;
} // namespace std

namespace taomp {

template <typename T,
          std::size_t align = std::hardware_destructive_interference_size>
T *aligned_alloc(std::size_t num = 1) {
  auto *ret = reinterpret_cast<T *>(::aligned_alloc(align, sizeof(T) * num));
  assert(ret);
  return ret;
}

namespace internal {
inline std::atomic<unsigned> thread_count = 0;
inline thread_local unsigned thread_id;
} // namespace internal

inline void init_thread() {
  internal::thread_id =
      internal::thread_count.fetch_add(1, std::memory_order_acq_rel);
}

inline unsigned get_thread_id() { return internal::thread_id; }

inline void reset() {
  internal::thread_count.store(0, std::memory_order_release);
}

template <typename T, std::size_t Alignment = std::hardware_destructive_interference_size> class ThreadLocal {
  struct alignas(Alignment) ContainerT {
    T value;
    template <typename... Args>
    ContainerT(Args &... args) : value(std::forward<Args>(args)...) {}
    ContainerT() = default;
  };
  unsigned thread_num;
  ContainerT *tls;

public:
  template <typename... Args> ThreadLocal(unsigned thread_num, Args &... args) : thread_num(thread_num) {
    assert(thread_num);
    tls = taomp::aligned_alloc<ContainerT, Alignment>(thread_num);
    for (unsigned i = 0; i < thread_num; ++i) {
      new (tls + i) ContainerT{std::forward<Args>(args)...};
    }
  }
  ThreadLocal(unsigned thread_num) {
    assert(thread_num);
    tls = taomp::aligned_alloc<ContainerT, Alignment>(thread_num);
    for (unsigned i = 0; i < thread_num; ++i) {
      new (tls + i) ContainerT;
    }
  }
  T &get(unsigned tid = get_thread_id()) { return tls[tid].value; }
  T &operator[](unsigned tid) { return get(tid); }
  void set(unsigned tid, const T &v) { tls[tid] = v; }
  void set(const T & v) {
    set(get_thread_id(), v);
  }
  void set(unsigned tid, T &&v) { tls[tid] = std::move(v); }
  void set(T && v) {
    set(get_thread_id(), v);
  }
  ~ThreadLocal() {
    for (unsigned i = 0; i < thread_num; ++i) {
      tls[i].~ContainerT();
    }
    free(tls);
  }
};

namespace internal {
template <typename T, std::size_t N> struct MaskLeadingZero_impl {
  static T calc(T n) {
    return MaskLeadingZero_impl<T, N - 1>::calc(n | (n >> 1));
  }
};

template <typename T> struct MaskLeadingZero_impl<T, 1> {
  static T calc(T n) { return n; }
};
} // namespace internal

/**Let n be of unsigned type T, let v be the smallest number, which is a power
 * of 2 and not smaller than n, this template return v - 1
 */
template <typename T>
typename std::enable_if<std::is_unsigned<T>::value, T>::type
MaskLeadingZero(T n) {
  return internal::MaskLeadingZero_impl<T, sizeof(T) * CHAR_BIT>::calc(n);
}

template <typename T> T Mask(unsigned bits) { return (T(1) << bits) - T(1); }


using TimeStamp = uint64_t;
TimeStamp readCPUCycleCount() {
  /** shameless stolen from google/benchmark:src/cycleclock.h
   */
#if defined(__aarch64__)
  // System timer of ARMv8 runs at a different frequency than the CPU's.
  // The frequency is fixed, typically in the range 1-50MHz.  It can be
  // read at CNTFRQ special register.  We assume the OS has set up
  // the virtual timer properly.
  TimeStamp virtual_timer_value;
  asm volatile("mrs %0, cntvct_el0" : "=r"(virtual_timer_value));
  return virtual_timer_value;
#elif defined(__x86_64__) || defined(__amd64__)
  TimeStamp low, high;
  __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
  return (high << 32) | low;
#else
  return 0;
#endif
}

template <bool Enable> class LinearizationPoint;

template <> class LinearizationPoint<true> {
private:
  ThreadLocal<TimeStamp> before;
  ThreadLocal<TimeStamp> after;

public:
  LinearizationPoint(unsigned thread_num) : before(thread_num), after(thread_num) {}

  void linearizeBefore(unsigned tid = get_thread_id()) {
    before[tid] = readCPUCycleCount();
  }
  void linearizeAfter(unsigned tid = get_thread_id()) {
    after[tid] = readCPUCycleCount();
  }
  void linearizeHere(unsigned tid = get_thread_id()) {
    before[tid] = after[tid] = readCPUCycleCount();
  }
  TimeStamp getLinearizationPoint(unsigned tid = get_thread_id()) {
    return before[tid];
  }
  TimeStamp getLinearizationPointBefore(unsigned tid = get_thread_id()) {
    return before[tid];
  }
  TimeStamp getLinearizationPointAfter(unsigned tid = get_thread_id()) {
    return after[tid];
  }
};

template <> class LinearizationPoint<false> {
public:
  LinearizationPoint(unsigned) {}
  void linearizeBefore(unsigned = 0) {}
  void linearizeAfter(unsigned = 0) {}
  void linearizeHere(unsigned = 0) {}
  TimeStamp getLinearizationPoint(unsigned = 0) { return 0; }
  TimeStamp getLinearizationPointBefore(unsigned = 0) { return 0; }
  TimeStamp getLinearizationPointAfter(unsigned = 0) { return 0; }
};
} // namespace taomp
