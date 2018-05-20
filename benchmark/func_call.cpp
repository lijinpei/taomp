#include "benchmark/benchmark.h"

#include <chrono>

void f(int, int, int, int);

void BM_FuncCall(benchmark::State &state) {
  for (auto _ : state) {
    f(1, 2, 3, 4);
  }
}

BENCHMARK(BM_FuncCall);
BENCHMARK_MAIN();

