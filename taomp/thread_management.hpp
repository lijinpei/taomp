#pragma once

#include <atomic>

namespace taomp {

namespace internal {
inline std::atomic<unsigned> thread_count = 0;
inline thread_local unsigned thread_id;
}

inline void init_thread() {
  internal::thread_id =
      internal::thread_count.fetch_add(1, std::memory_order_acq_rel);
}

inline unsigned get_thread_id() { return internal::thread_id; }

inline void reset() {
  internal::thread_count.store(0, std::memory_order_release);
}

} // namespace taomp
