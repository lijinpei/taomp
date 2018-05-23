#include "taomp/ms_queue.hpp"

#include <memory>

const unsigned thread_num = 8;
taomp::MSQueue<int> queue(thread_num);
int main() {
}
