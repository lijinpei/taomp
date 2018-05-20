#pragma once

#include "utils.hpp"

namespace taomp {
template <typename T>
class ThreadLocalAllocator {
  T* *arr;
public: 
  ThreadLocalAllocator(unsigned thread_num) : arr(new T*[thread_num]{}) {
  }
  T* allocate() {
    return arr[tm::get_thread_id()];
  }
  void deallocate(T* p) {
    arr[tm::get_thread_id()] = p;
  }
};
}
