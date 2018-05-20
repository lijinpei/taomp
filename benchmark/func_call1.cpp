#include "benchmark/benchmark.h"

void i() {
  benchmark::ClobberMemory();
}

void h() {
  i();
  benchmark::ClobberMemory();
}

void g() {
  h();
  benchmark::ClobberMemory();
}

void f(int, int, int, int) {
  g();
  benchmark::ClobberMemory();
}
