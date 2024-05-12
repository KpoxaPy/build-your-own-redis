#include "events.h"

#include <iostream>

EventLoopPtr EventLoop::make() {
  return std::make_shared<EventLoop>();
}

EventLoop::EventLoop(std::size_t max_unqueue_events)
  : _max_unqueue_events(max_unqueue_events)
{
}

void EventLoop::post(Func func) {
  this->_onetime_jobs.emplace(std::move(func));
}

void EventLoop::repeat(Func func) {
  this->_repeated_jobs.emplace_back(std::move(func));
}

void EventLoop::start() {
  while (true) {
    while (this->_onetime_jobs.size() > 0) {
      try {
        auto job = std::move(this->_onetime_jobs.front());
        this->_onetime_jobs.pop();
        job();
      } catch (const std::exception& exception) {
        std::cerr << "EventLoop caught exception while onetime job: " << std::endl
          << exception.what() << std::endl;
      }
    }

    for (const auto& job: this->_repeated_jobs) {
      try {
        job();
      } catch (const std::exception& exception) {
        std::cerr << "EventLoop caught exception while repeating job: " << std::endl
          << exception.what() << std::endl;
      }
    }

    std::size_t unqueue_size = this->_max_unqueue_events;
    if (unqueue_size == 0 || unqueue_size > this->_events.size()) {
      unqueue_size = this->_events.size();
    }

    for (std::size_t i = 0; i < unqueue_size; ++i) {
      try {
        auto event = std::move(this->_events.front());
        this->_events.pop();
        event();
      } catch (const std::exception& exception) {
        std::cerr << "EventLoop caught exception while processing event: " << std::endl
                  << exception.what() << std::endl;
      }
    }
  }
}
