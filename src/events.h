#pragma once

#include "event_types.h"

#include <functional>
#include <list>
#include <memory>
#include <queue>
#include <unordered_map>
#include <utility>

class EventLoopManager;
using EventLoopManagerPtr = std::shared_ptr<EventLoopManager>;

class EventLoopManager {
public:
  using RepeatingFunc = std::function<void()>;
  using EventFunc = std::function<void(const Event&)>;

  static constexpr std::size_t MAX_UNQUEUE_EVENTS_DEFAULT = -1;

  static EventLoopManagerPtr make();

  EventLoopManager(std::size_t max_unqueue_events = MAX_UNQUEUE_EVENTS_DEFAULT);

  EventDescriptor make_desciptor();

  void repeat(RepeatingFunc);

  void listen(EventDescriptor descriptor, EventFunc);

  void post(Event);

  void start();

private:
  EventDescriptor _last = EventLast;
  std::list<RepeatingFunc> _repeated_jobs;
  std::unordered_map<EventDescriptor, std::list<EventFunc>> _listeners;

  const std::size_t _max_unqueue_events;
  std::queue<Event> _events;
};