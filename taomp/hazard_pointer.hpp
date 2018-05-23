#pragma once

#include "thread_management.hpp"
#include "utils.hpp"

#include "llvm/ADT/DenseSet.h"
#include <atomic>
#include <forward_list>
#include <iostream>

namespace taomp {
template <typename AllocatorTy> struct HazardPointer {
  using HpTy = void *;
  // it is impossible to have large number of threads or hps
  unsigned total_hp_num, thread_num;
  std::atomic<HpTy> *hps;
  struct ThreadLocal {
    HpTy *start;
    unsigned size;
  };
  ThreadLocal *tls;
  HpTy *tls_storage;
  AllocatorTy &allocator;
  unsigned deallocate_threshold;
  unsigned storage_per_thread;
  unsigned defaultDeallocateThreshold() {
    // guarantee that after scan(), at least one slot is empty
    return total_hp_num + 1;
  }
  void scan(ThreadLocal &tl) {
    assert(tl.start >= tls_storage);
    llvm::DenseSet<HpTy> hp_set(total_hp_num);
    for (unsigned i = 0; i < total_hp_num; ++i) {
      HpTy hp = hps[i].load(std::memory_order_relaxed);
      if (hp) {
        hp_set.insert(hp);
      }
    }
    unsigned i1 = 0, i2 = 0;
    for (; i1 < tl.size; ++i1) {
      HpTy hp = tl.start[i1];
      if (hp_set.find(hp) == hp_set.end()) {
        allocator.deallocate(
            reinterpret_cast<typename AllocatorTy::value_type *>(hp));
      } else {
        tl.start[i2++] = hp;
      }
    }
    tl.size = i2;
    assert(tl.size < deallocate_threshold);
  }

public:
  HazardPointer(unsigned thread_num, unsigned total_hp_num,
                AllocatorTy &allocator, unsigned deallocate_threshold_ = 0)
      : total_hp_num(total_hp_num), thread_num(thread_num),
        hps(new std::atomic<HpTy>[total_hp_num] {}),
        tls(new ThreadLocal[thread_num]{}), allocator(allocator),
        deallocate_threshold(deallocate_threshold_) {
    if (deallocate_threshold == 0) {
      deallocate_threshold = defaultDeallocateThreshold();
    }
    assert(deallocate_threshold >= defaultDeallocateThreshold());
    unsigned tmp_ = deallocate_threshold * sizeof(HpTy);
    unsigned tmp1_ = tmp_ / std::hardware_constructive_interference_size;
    unsigned tmp2_ = tmp_ % std::hardware_constructive_interference_size;
    storage_per_thread = (tmp2_ ? tmp1_ + 1 : tmp1_) *
                         std::hardware_constructive_interference_size;
    tls_storage =
        reinterpret_cast<HpTy *>(::malloc(thread_num * storage_per_thread));
    assert(tls_storage);
    uintptr_t tlss_ = reinterpret_cast<uintptr_t>(tls_storage);
    for (unsigned i = 0; i < thread_num; ++i) {
      tls[i].start = reinterpret_cast<HpTy *>(tlss_);
      tlss_ += storage_per_thread;
      ThreadLocal &tl = tls[i];
      assert(!tl.size);
      assert(tl.start >= tls_storage);
      assert(tl.start < tls_storage + storage_per_thread * thread_num);
      assert(tl.start + deallocate_threshold <=
             tls_storage + storage_per_thread * thread_num);
      // tls_ += storage_per_thread;
    }
  }

  ~HazardPointer() {
    for (unsigned i = 0; i < thread_num; ++i) {
      forcedDeallocate(i);
    }
    delete[] hps;
    delete[] tls;
    free(tls_storage);
  }

  void forcedDeallocate(unsigned tid) {
    ThreadLocal &tl = tls[tid];
    for (unsigned i = 0; i < tl.size; ++i) {
      allocator.deallocate(
          reinterpret_cast<typename AllocatorTy::value_type *>(tl.start[i]));
    }
    tl.size = 0;
  }

  void forcedDeallocate() { forcedDeallocate(get_thread_id()); }

  template <typename T> void retire(T *p_) {
    HpTy p = reinterpret_cast<HpTy>(p_);
    unsigned tid = get_thread_id();
    assert(tid < thread_num);
    ThreadLocal &tl = tls[tid];
    assert(tl.size < deallocate_threshold);
    assert(tl.start + tl.size + 1 <=
           tls_storage + storage_per_thread * thread_num);
    tl.start[tl.size++] = p;
    if (tl.size == deallocate_threshold) {
      scan(tl);
      assert(tl.size < deallocate_threshold);
    }
  }

  template <typename T>
  void preserve(unsigned index, T *hp_,
                std::memory_order order = std::memory_order_relaxed) {
    assert(index < total_hp_num);
    hps[index].store(reinterpret_cast<HpTy>(hp_), order);
  }

  template <typename T>
  T *get(unsigned index, std::memory_order order = std::memory_order_relaxed) {
    assert(index < total_hp_num);
    return reinterpret_cast<T *>(hps[index].load(order));
  }

  template <typename T> std::atomic<T *> *getHp(unsigned index) {
    assert(index < total_hp_num);
    return reinterpret_cast<std::atomic<T *> *>(hps + index);
  }
};

} // namespace taomp

