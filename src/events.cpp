#include "events.h"

#include <iostream>

EventLoopManagerPtr EventLoopManager::make() {
  return std::make_shared<EventLoopManager>();
}

EventLoopManager::EventLoopManager(std::size_t max_unqueue_events)
  : _max_unqueue_events(max_unqueue_events)
{
}

EventDescriptor EventLoopManager::make_desciptor() {
  return this->_last++;
}

void EventLoopManager::repeat(Func func) {
  this->_repeated_jobs.emplace_back(std::move(func));
}

void EventLoopManager::post(Func func) {
  this->_onetime_jobs.emplace(std::move(func));
}

void EventLoopManager::listen(EventDescriptor descriptor, EventFunc func) {
  auto& listeners = this->_listeners[descriptor];
  listeners.emplace_back(std::move(func));
}

void EventLoopManager::post(EventPtr event) {
  this->_events.emplace(std::move(event));
}

void EventLoopManager::start() {
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
      auto& event = this->_events.front();
      auto event_descriptor = event->descriptor(); 

      auto listeners_it = this->_listeners.find(event_descriptor);
      if (listeners_it != this->_listeners.end()) {
        // std::cerr << "Unqueued event(" << event_descriptor << ") with " << listeners_it->second.size() << " listeners" << std::endl;
        for (const auto& listener: listeners_it->second) {
          try {
            listener(*event);
          } catch (const std::exception& exception) {
            std::cerr << "EventLoop caught exception while processing event: " << std::endl
              << exception.what() << std::endl;
          }
        }
      } else {
        std::cerr << "Unqueued event(" << event_descriptor << ") with 0 listeners" << std::endl;
      }

      this->_events.pop();
    }
  }
}
