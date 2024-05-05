#pragma once

#include <functional>
#include <list>
#include <memory>
#include <queue>
#include <unordered_map>
#include <utility>

using EventDescriptor = std::size_t;
constexpr EventDescriptor UNDEFINED_EVENT = 0;

class EventLoop;
using EventLoopPtr = std::shared_ptr<EventLoop>;

class Event {
private:
  friend EventLoop;
  EventDescriptor _descriptor = UNDEFINED_EVENT;
};
using EventPtr = std::unique_ptr<Event>;

class EventLoop {
public:
  using Func = std::function<void()>;
  using EventFunc = std::function<void(const Event&)>;

  static constexpr std::size_t MAX_UNQUEUE_EVENTS_DEFAULT = -1;

  static EventLoopPtr make();

  EventLoop(std::size_t max_unqueue_events = MAX_UNQUEUE_EVENTS_DEFAULT);

  EventDescriptor make_desciptor();

  void repeat(Func);
  void post(Func);

  void listen(EventDescriptor, EventFunc);

  template <typename EventType, typename... Args>
  inline void post(EventDescriptor descriptor, Args&&... args) {
    this->post(descriptor, std::make_unique<EventType>(std::forward<Args>(args)...));
  }
  void post(EventDescriptor, EventPtr);

  void start();

private:
  EventDescriptor _last = UNDEFINED_EVENT + 1;
  std::list<Func> _repeated_jobs;
  std::queue<Func> _onetime_jobs;
  std::unordered_map<EventDescriptor, std::list<EventFunc>> _listeners;

  const std::size_t _max_unqueue_events;
  std::queue<EventPtr> _events;
};