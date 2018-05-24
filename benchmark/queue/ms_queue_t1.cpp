#include "taomp/ms_queue.hpp"
#include "cds/init.h"
#include "cds/gc/hp.h"
#include "cds/container/msqueue.h"
#include "benchmark/benchmark.h"
#include <memory>
#include <optional>

const int N = 1000;
const int thread_num = 10;
std::optional<int> op(1);

void BM_ForwardList(benchmark::State &state) {
  std::forward_list<int> list;
  for (auto _ : state) {
    for (int i = 0; i < N; ++i) {
      list.push_front(i);
    }
    for (int i = 0; i < N; ++i) {
      list.pop_front();
    }
  }
}

void BM_MSQueue(benchmark::State & state) {
  taomp::MSQueue<int> queue(thread_num);
  for (auto _ : state) {
    for (int i = 0; i < N; ++i) {
      queue.enqueue(i);
    }
    for (int i = 0; i < N; ++i) {
      queue.dequeue();
    }
  }
}

void BM_LIBCDS_MSQUEUE(benchmark::State & state) {
  cds::container::MSQueue<cds::gc::HP, int> queue;
  cds::gc::hp::GarbageCollector::Construct(decltype(queue)::c_nHazardPtrCount, 1,
                                           16);
  cds::threading::Manager::attachThread();
  int p;
  for (auto _ : state) {
    for (int i = 0; i < 10; ++i) {
      queue.enqueue(i);
    }
    /*
    for (int i = 0; i < 10; ++i) {
      queue.dequeue(p);
    }
    */
  }
  cds::threading::Manager::detachThread();
  cds::Terminate();
}

BENCHMARK(BM_ForwardList);
BENCHMARK(BM_MSQueue);
//BENCHMARK(BM_LIBCDS_MSQUEUE);
BENCHMARK_MAIN();
