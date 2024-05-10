#include "events.h"

#include <iostream>

using namespace _NS_Events;

EventLoopPtr EventLoop::make() {
  return std::make_shared<EventLoop>();
}

EventLoop::EventLoop(std::size_t max_unqueue_events)
  : _max_unqueue_events(max_unqueue_events)
{
}

void EventLoop::repeat(Func func) {
  this->_repeated_jobs.emplace_back(std::move(func));
}

void EventLoop::post(Func func) {
  this->_onetime_jobs.emplace(std::move(func));
}

std::pair<EventDescriptor, SlotId> EventLoop::listen(Slot slot) {
  const auto descriptor = this->_next_descriptor++;
  const auto slot_id = this->_next_slot_id++;

  auto it = this->_slots.find(descriptor);
  if (it != this->_slots.end()) {
    this->unlisten(it->second);
  }

  this->_slot_registry.emplace(slot_id, std::move(slot));
  this->_slots[descriptor] = slot_id;

  return {descriptor, slot_id};
}

void EventLoop::unlisten(SlotId id) {
  this->_slot_registry.erase(id);
}

void EventLoop::emit(Signal signal) {
  this->_events.emplace(std::move(signal));
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
      auto signal = std::move(this->_events.front());
      this->_events.pop();

      const auto id_it = this->_slots.find(signal._descriptor);
      if (id_it == this->_slots.end()) {
        std::cerr << "Unqueued event(id=" << signal._descriptor << ") with no slot associated" << std::endl;
        continue;
      }

      const auto slot_it = this->_slot_registry.find(id_it->second);
      if (slot_it == this->_slot_registry.end()) {
        std::cerr << "Unqueued event(id=" << signal._descriptor << ") with removed slot" << std::endl;
        continue;
      }

      try {
        signal.deliver(slot_it->second);
      } catch (const std::exception& exception) {
        std::cerr << "EventLoop caught exception while processing event: " << std::endl
                  << exception.what() << std::endl;
      }
    }
  }
}
