#pragma once

#include "taomp/hazard_pointer.hpp"
#include "taomp/utils.hpp"
#include <atomic>
#include <memory>
#include <optional>

namespace taomp {

template <typename Ty> struct MSQueueNode {
  std::atomic<MSQueueNode *> next;
  Ty value;
  MSQueueNode() : next(nullptr) {}
};

template <typename Ty,
          bool GetLinearizationPoint = false,
          typename GC = HazardPointer<std::allocator<MSQueueNode<Ty>>>>
class MSQueue : public LinearizationPoint<GetLinearizationPoint> {
  using Node = MSQueueNode<Ty>;
  GC gc;
  Node *sentinel;
  std::atomic<Node *> tail, head;

public:
  MSQueue(unsigned thread_num)
      : LinearizationPoint<GetLinearizationPoint>(thread_num),
        gc(thread_num, 2 * thread_num), sentinel(gc.allocate(1)),
        tail(sentinel), head(sentinel) {
    sentinel->next = nullptr;
  }

  void enqueue(Ty value) {
    unsigned tid = get_thread_id();
    std::atomic<Node *> &hp = gc.template get<Node>(tid << 1);
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
      Node *next = t->next.load();
      if (next) {
        tail.compare_exchange_strong(t, next);
        continue;
      }
      Node *t1 = nullptr;
      this->linearizeBefore();
      if (t->next.compare_exchange_strong(t1, node)) {
        this->linearizeAfter();
        break;
      }
    }
    tail.compare_exchange_strong(t, node);
    hp.store(nullptr);
  }

  std::optional<Ty> dequeue() {
    unsigned tid = get_thread_id();
    std::atomic<Node *> &hp1 = gc.template get<Node>(tid << 1);
    std::atomic<Node *> &hp2 = gc.template get<Node>((tid << 1) + 1);
    Ty value;
    Node *h;
    while (true) {
      h = head.load();
      hp1.store(h);
      if (head.load() != h) {
        continue;
      }
      Node *t = tail.load();
      this->linearizeBefore();
      Node *next = h->next.load();
      this->linearizeAfter();
      hp2.store(next);
      if (head.load() != h) {
        continue;
      }
      if (h == t) {
        if (!next) {
          return {};
        } else {
          tail.compare_exchange_strong(t, next);
          continue;
        }
      }
      if (!next) {
        return {};
      }
      value = next->value;
      this->linearizeBefore();
      if (head.compare_exchange_strong(h, next)) {
        this->linearizeAfter();
        break;
      }
    }
    hp1.store(nullptr);
    hp2.store(nullptr);
    gc.retire(h);
    return value;
  }

  Node *end() { return sentinel; }
};

} // namespace taomp
