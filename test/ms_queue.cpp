#include "taomp/ms_queue.hpp"
#include "taomp/utils.hpp"

#include <deque>
#include <memory>
#include <optional>
#include <random>
#include <limits>
#include <vector>
#include <thread>
#include <algorithm>

const unsigned thread_num = 8;
const int N = 10000;
taomp::MSQueue<int, true>
    queue(thread_num);

struct Event {
  enum {
    EK_Enqueue,
    EK_Dequeue
  } kind;
  std::optional<int> result;
  taomp::TimeStamp before, after;
  void dump(std::ostream & os) {
    os << before << ' ' << after << " : ";
    if (kind == EK_Dequeue) {
      os << "dequeue(";
      if (result) {
        os << result.value();
      } else {
        os << "null";
      }
      os << ')';
    } else {
      os << "enqueue(" << result.value() << ')';
    }
  }
};

taomp::ThreadLocal<std::vector<Event>> tls(thread_num, N);

void runTest() {
  taomp::init_thread();
  unsigned tid = taomp::get_thread_id();
  std::vector<Event> & tl = tls[tid];
  std::random_device r;
  std::default_random_engine e1(r());
  std::uniform_int_distribution<int> dist(0, 1);
  for (int i = 0; i < N; ++i) {
    if (dist(e1)) {
      int v = (i << 8) + tid;
      queue.enqueue(v);
      tl[i].kind = Event::EK_Enqueue;
      tl[i].result = v;
      tl[i].before = queue.getLinearizationPointBefore(tid);
      tl[i].after = queue.getLinearizationPointAfter(tid);
    } else {
      auto res = queue.dequeue();
      tl[i].kind = Event::EK_Dequeue;
      tl[i].result = res;
      tl[i].before = queue.getLinearizationPointBefore(tid);
      tl[i].after = queue.getLinearizationPointAfter(tid);
    }
  }
}

std::vector<std::thread*> threads(thread_num);
int main() {
  queue.enqueue(1);
  queue.dequeue();
  for (unsigned i = 1; i < thread_num; ++i) {
    threads[i] = new std::thread(runTest);
  }
  runTest();
  for (unsigned i = 1; i < thread_num; ++i) {
    threads[i]->join();
  }
  std::vector<Event> events(thread_num * N);
  for (unsigned i = 0; i < thread_num; ++i) {
    std::vector<Event> & tl = tls[i];
    std::copy(tl.begin(), tl.end(), events.begin() + i * N);
  }
  std::cerr << "dump finish\n";
  std::cerr << events.size() << std::endl;
  size_t s = events.size();
  std::sort(events.begin(), events.end(), [](Event & e1, Event & e2) {return e1.before < e2.before ;});
  int overlap_count = 0;
  for (unsigned i = 1; i < s; ++i) {
    if (events[i - 1].after > events[i].before) {
      ++overlap_count;
    }
  }
  std::cerr << "overlap: " << overlap_count << std::endl;
  /*
  for (auto & e : events) {
    e.dump(std::cout);
    std::cout << std::endl;
  }
  */
  std::deque<int> queue1;
  auto output = [](std::optional<int> op, std::ostream & os) {
    if (op) {
      os << op.value();
    } else {
      os << "null";
    }
  };
  (void)output;
  /*
  for (auto & e : events) {
    ++i;
    if (e.kind == Event::EK_Dequeue) {
      std::optional<int> result;
      if (!queue1.empty()) {
        result = queue1.front();
        queue1.pop_front();
      }
      if (result != e.result) {
        std::cout << i << ' ';
        output(result, std::cout);
        std::cout << ' ';
        output(e.result, std::cout);
        std::cout << std::endl;
        break;
      }
    } else {
      queue1.push_back(e.result.value());
    }
  }
  */
  std::vector<int> v1, v2;
  for (auto & e : events) {
    if (e.kind == Event::EK_Dequeue) {
      if (e.result) {
        v2.push_back(e.result.value());
      }
    } else {
      v1.push_back(e.result.value());
    }
  }
  while (true) {
    auto res = queue.dequeue();
    if (!res) {
      break;
    }
    v2.push_back(res.value());
  }
  std::sort(v1.begin(), v1.end());
  std::sort(v2.begin(), v2.end());
  std::cout << v1.size() << ' ' << v2.size() << '\n';
  assert(v1.size() == v2.size());
  size_t s1 = v1.size();
  for (size_t i = 0; i < s1; ++i) {
    assert(v1[i] == v2[i]);
  }
}
