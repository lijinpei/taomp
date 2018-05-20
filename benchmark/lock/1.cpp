#include "taomp/allocator.hpp"
#include "taomp/lock.hpp"
#include "taomp/lock_third_party.hpp"
#include <benchmark/benchmark.h>
#include <cassert>
#include <iostream>
#include <mutex>
#include <pthread.h>
#include <stdlib.h>
#include <chrono>
#include "tbb/spin_mutex.h"

using namespace std::chrono_literals;

template <class Lock, int N>
static void BM_LockHighContention0(benchmark::State &state) {
  static Lock lock;
  static size_t count;
  if (!state.thread_index) {
    count = 0;
  }
  taomp::ExpBackoff backoffer(100ns, 10us);
  for (auto _ : state) {
    for (int i = 0; i < N; ++i) {
      lock.lock(backoffer);
      ++count;
      lock.unlock();
    }
  }
  if (!state.thread_index) {
    if (!(N * state.threads * state.iterations() == count)) {
      std::cout << N * state.threads << ' ' << count << '\n';
      assert(false);
    }
  }
}

template <class Lock, int N>
static void BM_LockHighContention(benchmark::State &state) {
  static Lock lock;
  static size_t count;
  if (!state.thread_index) {
    count = 0;
  }
  for (auto _ : state) {
    for (int i = 0; i < N; ++i) {
      std::lock_guard<Lock> X(lock);
      ++count;
    }
  }
  if (!state.thread_index) {
    if (!(N * state.threads * state.iterations() == count)) {
      std::cout << N * state.threads << ' ' << count << '\n';
      assert(false);
    }
  }
}

template <class Lock, int N>
static void BM_LockHighContention1(benchmark::State &state) {
  static Lock lock(state.threads);
  static size_t count;
  if (!state.thread_index) {
    count = 0;
  }
  for (auto _ : state) {
    for (int i = 0; i < N; ++i) {
      std::lock_guard<Lock> X(lock);
      ++count;
    }
  }
  if (!state.thread_index) {
    if (!(N * state.threads * state.iterations() == count)) {
      std::cout << N * state.threads << ' ' << count << '\n';
      assert(false);
    }
  }
}

template <class Lock, int N>
static void BM_LockHighContention2(benchmark::State &state) {
  static Lock lock;
  static size_t count;
  static std::atomic<bool> flag = false;
  using QNode = typename Lock::QNode;
  QNode *in_node = taomp::aligned_alloc<typename Lock::QNode>(), *out_node;
  if (!state.thread_index) {
    count = 0;
    QNode* init_tail = taomp::aligned_alloc<typename Lock::QNode>();
    init_tail->store(false, std::memory_order_relaxed);
    lock.reset(init_tail);
    flag.store(true, std::memory_order_release);
  } else {
    while (!flag.load(std::memory_order_acquire)) {
      continue;
    }
  }
  for (auto _ : state) {
    for (int i = 0; i < N; ++i) {
      out_node = lock.lock(in_node);
      ++count;
      lock.unlock(in_node);
      in_node = out_node;
    }
  }
  if (!state.thread_index) {
    if (!(N * state.threads * state.iterations() == count)) {
      std::cout << N * state.threads << ' ' << count << '\n';
      assert(false);
    }
  }
}

template <class Lock, int N>
static void BM_LockHighContention3(benchmark::State &state) {
  static Lock lock;
  static size_t count;
  static std::atomic<bool> flag = false;
  using QNode = typename Lock::QNode;
  QNode *in_node = taomp::aligned_alloc<typename Lock::QNode>();
  in_node->store(0, std::memory_order_release);
  if (!state.thread_index) {
    count = 0;
    flag.store(true, std::memory_order_release);
  } else {
    while (!flag.load(std::memory_order_acquire)) {
      continue;
    }
  }
  for (auto _ : state) {
    for (int i = 0; i < N; ++i) {
      lock.lock(in_node);
      ++count;
      lock.unlock(in_node);
    }
  }
  if (!state.thread_index) {
    if (!(N * state.threads * state.iterations() == count)) {
      std::cout << N * state.threads << ' ' << count << '\n';
      assert(false);
    }
  }
}

const int N = 1024 * 1024 * 2;
const int P = 4;
BENCHMARK_TEMPLATE(BM_LockHighContention0, taomp::TASLock, N)->Threads(P);
BENCHMARK_TEMPLATE(BM_LockHighContention0, taomp::TTASLock, N)->Threads(P);
BENCHMARK_TEMPLATE(BM_LockHighContention, taomp::TASLock, N)->Threads(P);
BENCHMARK_TEMPLATE(BM_LockHighContention, taomp::TTASLock, N)->Threads(P);
BENCHMARK_TEMPLATE(BM_LockHighContention, tbb::spin_mutex, N)->Threads(P);
BENCHMARK_TEMPLATE(BM_LockHighContention1, taomp::ArrayLock, N)->Threads(P);
BENCHMARK_TEMPLATE(BM_LockHighContention, taomp::PThreadSpinLock, N)
    ->Threads(P);
BENCHMARK_TEMPLATE(BM_LockHighContention, std::mutex, N)->Threads(P);
BENCHMARK_TEMPLATE(BM_LockHighContention2, taomp::CLHLock, N)->Threads(P);
BENCHMARK_TEMPLATE(BM_LockHighContention3, taomp::MCSLock, N)->Threads(P);
BENCHMARK_MAIN();
