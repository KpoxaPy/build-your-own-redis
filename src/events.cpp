#include "events.h"

#include <iostream>

EventLoopPtr EventLoop::make() {
  return std::make_shared<EventLoop>();
}

EventLoop::EventLoop(std::size_t max_unqueue_events)
  : _max_unqueue_events(max_unqueue_events)
{
}

EventDescriptor EventLoop::make_desciptor() {
  return this->_last++;
}

void EventLoop::repeat(Func func) {
  this->_repeated_jobs.emplace_back(std::move(func));
}

void EventLoop::post(Func func) {
  this->_onetime_jobs.emplace(std::move(func));
}

void EventLoop::listen(EventDescriptor descriptor, EventFunc func) {
  if (descriptor == UNDEFINED_EVENT) {
    return;
  }

  auto& listeners = this->_listeners[descriptor];
  listeners.emplace_back(std::move(func));
}

void EventLoop::post(EventDescriptor descriptor, EventPtr event) {
  if (descriptor == UNDEFINED_EVENT) {
    return;
  }

  event->_descriptor = descriptor;
  this->_events.emplace(std::move(event));
}

void EventLoop::start() {
  while (true) {
    for (auto& job: this->_repeated_jobs) {
      try {
        job();
      } catch (const std::exception& exception) {
        std::cerr << "EventLoop caught exception while repeating job: " << std::endl
          << exception.what() << std::endl;
      }
    }

    while (this->_onetime_jobs.size() > 0) {
      try {
        this->_onetime_jobs.front()();
        this->_onetime_jobs.pop();
      } catch (const std::exception& exception) {
        std::cerr << "EventLoop caught exception while onetime job: " << std::endl
          << exception.what() << std::endl;
      }
    }

    std::size_t unqueue_size = this->_max_unqueue_events;
    if (unqueue_size == 0 || unqueue_size > this->_events.size()) {
      unqueue_size = this->_events.size();
    }

    for (std::size_t i = 0; i < unqueue_size; ++i) {
      const auto& event = *this->_events.front();

      auto listeners_it = this->_listeners.find(event._descriptor);
      if (listeners_it != this->_listeners.end()) {
        for (const auto& listener: listeners_it->second) {
          try {
            listener(event);
          } catch (const std::exception& exception) {
            std::cerr << "EventLoop caught exception while processing event: " << std::endl
              << exception.what() << std::endl;
          }
        }
      } else {
        std::cerr << "Unqueued event(" << event._descriptor << ") with 0 listeners" << std::endl;
      }

      this->_events.pop();
    }
  }
}
