#pragma once

#include "taomp/hazard_pointer.hpp"
#include "taomp/thread_management.hpp"
#include <atomic>
#include <memory>
#include <optional>

namespace taomp {

template <typename Ty>
struct MSQueueNode {
  std::atomic<MSQueueNode*> next;
  Ty value;
  MSQueueNode() : next(nullptr) {}
};

template <typename Ty, typename GC = HazardPointer<std::allocator<MSQueueNode<Ty>>>>
class MSQueue {
  using Node = MSQueueNode<Ty>;
  Node sentinel;
  std::atomic<Node*> tail, head;
  GC gc;
public:
  MSQueue(unsigned thread_num)
      : tail(&sentinel), head(&sentinel), gc(thread_num, 2 * thread_num) {}
  void enqueue(Ty value) {
    unsigned tid = get_thread_id();
    std::atomic<Ty*> & hp = gc.get(tid << 1);
    Node *node = gc.allocate(1);
    node->value = value;
    node->next = nullptr;
    Node *t;
    while (true) {
      t = tail.load();
      hp.store(t);
      if (tail.load() != t) {
        continue;
      }
      Node* next = t->next.load();
      if (next) {
        tail.compare_and_exchange_strong(t, next);
        continue;
      }
      Node* t1 = nullptr;
      if (t->next.compare_and_exchange_strong(t1, node)) {
        break;
      }
    }
    tail.compare_and_exchange_strong(t, node);
    hp.store(nullptr);
  }
  std::optional<Ty> dequeue() {
    unsigned tid = get_thread_id();
    std::atomic<Ty*> & hp1 = gc.get(tid << 1);
    std::atomic<Ty*> & hp2 = gc.get((tid << 1) + 1);
    Ty value;
    Node* h;
    while (true) {
      h = head.load();
      hp1.store(h);
      if (head.load() != h) {
        continue;
      }
      Node * t = tail.load();
      Node* next = h->next.load();
      hp2.store(next);
      if (head.load() != h) {
        continue;
      }
      if (!next) {
        return {};
      }
      if (h == t) {
        tail.compare_and_exchange_strong(t, next);
        continue;
      }
      value = next.value;
      if (head.compare_and_exchange_strong(h, next)) {
        break;
      }
    }
    hp1.store(nullptr);
    hp2.store(nullptr);
    gc.retire(h);
    return value;
  }
};

}
