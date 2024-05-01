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

void EventLoopManager::repeat(RepeatingFunc func) {
  this->_repeated_jobs.emplace_back(std::move(func));
}

void EventLoopManager::listen(EventDescriptor descriptor, EventFunc func) {
  auto& listeners = this->_listeners[descriptor];
  listeners.emplace_back(std::move(func));
}

void EventLoopManager::post(Event event) {
  this->_events.emplace(std::move(event));
}

void EventLoopManager::start() {
  while (true) {
    for (auto& job: this->_repeated_jobs) {
      job();
    }

    std::size_t unqueue_size = this->_max_unqueue_events;
    if (unqueue_size == 0 || unqueue_size > this->_events.size()) {
      unqueue_size = this->_events.size();
    }

    for (std::size_t i = 0; i < unqueue_size; ++i) {
      auto& event = this->_events.front();
      auto event_descriptor = event.descriptor(); 

      auto listeners_it = this->_listeners.find(event_descriptor);
      if (listeners_it != this->_listeners.end()) {
        for (const auto& listener: listeners_it->second) {
          try {
            listener(event);
          } catch (const std::exception& exception) {
            std::cerr << "EventLoop caught exception while processing event: " << std::endl
              << exception.what() << std::endl;
          }
        }
      }

      this->_events.pop();
    }
  }
}
