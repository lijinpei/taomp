#include "benchmark/benchmark.h"

#include <chrono>

template <typename ClockTy>
void BM_CLockNow(benchmark::State &state) {
  for (auto _ : state) {
    ClockTy::now();
  }
}

BENCHMARK_TEMPLATE(BM_CLockNow, std::chrono::system_clock);
BENCHMARK_TEMPLATE(BM_CLockNow, std::chrono::steady_clock);
BENCHMARK_TEMPLATE(BM_CLockNow, std::chrono::high_resolution_clock);
BENCHMARK_MAIN();

