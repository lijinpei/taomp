#include "taomp/hazard_pointer.hpp"
#include "taomp/thread_management.hpp"

#include "mcheck.h"
#include "pthread.h"
#include <atomic>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

#define OUTPUT(x)                                                              \
  {                                                                            \
    pthread_spin_lock(&output_lock);                                           \
    x;                                                                         \
    pthread_spin_unlock(&output_lock);                                         \
  }

pthread_spinlock_t output_lock;
pthread_barrier_t barrier;
const int thread_num = 8;
const int hp_per_thread = 10;
const int N1 = 10000;
using PointeeTy = int;

std::vector<std::atomic<PointeeTy *>> pointers(N1);
class MyAllocator {
public:
  using value_type = PointeeTy;
  PointeeTy *allocate() { return nullptr; }
  void deallocate(PointeeTy *p) {
    *p = 1;
  }
};

MyAllocator allocator;
taomp::HazardPointer HP(thread_num, thread_num *hp_per_thread, allocator);

void runTest(int start_position) {
  taomp::init_thread();
  int tid = taomp::get_thread_id();
  OUTPUT(std::cerr << "thread " << tid << "start\n");
  int hp_count = hp_per_thread;
  int visit_count = 0;
  int start_index = tid * hp_per_thread;
  for (;hp_count; ++start_position) {
    if (start_position == N1) {
      start_position = 0;
    }
    if (++visit_count > N1) {
      break;
    }
    PointeeTy *ptr = pointers[start_position].load();
    if (!ptr) {
      continue;
    }
    HP.preserve(start_index + hp_count - 1, ptr);
    if (!pointers[start_position].load()) {
      HP.preserve<int>(start_index + hp_count - 1, nullptr);
      continue;
    }
    --hp_count;
  }
  for (int visit_count = 0; visit_count < N1; ++visit_count, ++start_position) {
    if (start_position == N1) {
      start_position = 0;
    }
    PointeeTy *ptr =
        pointers[start_position].exchange(nullptr);
    if (ptr) {
      HP.retire(ptr);
    }
  }
  OUTPUT(std::cerr << "thread " << tid << "stage 1 finish\n");
  pthread_barrier_wait(&barrier);
  for (int i = hp_count; i < hp_per_thread; ++i) {
    int v =*HP.get<PointeeTy>(start_index + i);
    if (v) {
      std::cerr << v << std::endl;
      std::cerr << "error\n";
    }
  }
  OUTPUT(std::cerr << "thread " << tid << "stage 2 finish\n");
  pthread_barrier_wait(&barrier);
  HP.forcedDeallocate();
}

int* ps = new int[N1]{};
int main() {
  pthread_spin_init(&output_lock, PTHREAD_PROCESS_PRIVATE);
  pthread_barrier_init(&barrier, nullptr, thread_num);
  // Seed with a real random value, if available
  std::random_device r;

  // Choose a random mean between 1 and 6
  std::default_random_engine e1(r());
  std::uniform_int_distribution<int> uniform_dist(0, N1 - 1);
  for (int i = 0; i < N1; ++i) {
    pointers[i] = ps + i;
    assert(!*pointers[i]);
  }
  std::vector<std::thread *> threads(thread_num);
  for (int i = 1; i < thread_num; ++i) {
    threads[i] = new std::thread(runTest, uniform_dist(e1));
  }
  runTest(uniform_dist(e1));
  for (int i = 1; i < thread_num; ++i) {
    threads[i]->join();
  }
  for (auto &p : pointers) {
    delete p;
  }
  pthread_spin_destroy(&output_lock);
  pthread_barrier_destroy(&barrier);
  std::cerr << "main thread finish\n";
}

