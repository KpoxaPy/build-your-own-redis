#include "events.h"

void EventLoopManager::post(Func func, RunType type, std::string name) {
  if (type == Once) {
    this->_events.emplace(std::move(func));
  } else if (type == Repeat) {
    this->_repeated_events.emplace_back(std::move(func));
  }
}

void EventLoopManager::start_loop() {
  while (true) {
    for (auto& event: this->_repeated_events) {
      event();
    }

    auto queue = std::move(this->_events);
    while (!queue.empty()) {
      queue.front()();
      queue.pop();
    }
  }
}
