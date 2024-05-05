#pragma once

#include "events.h"
#include "handler.h"

#include <memory>

class HandlerAddEvent : public Event {
public:
  HandlerAddEvent(int fd);

  int fd;
};

class HandlerRemoveEvent : public Event {
public:
  HandlerRemoveEvent(int fd);

  int fd;
};

class HandlersManager {
public:
  HandlersManager(EventLoopPtr event_loop);

  void connect_poller_add(EventDescriptor);
  void connect_poller_remove(EventDescriptor);
  EventDescriptor add_listener();
  EventDescriptor remove_listener();
  void start();

private:
  EventDescriptor _poller_add = UNDEFINED_EVENT;
  EventDescriptor _poller_remove = UNDEFINED_EVENT;
  EventDescriptor _add_listener = UNDEFINED_EVENT;
  EventDescriptor _remove_listener = UNDEFINED_EVENT;
  EventLoopPtr _event_loop;

  std::unordered_map<int, Handler> _handlers;

  void add(int fd);
  void remove(int fd);
};
using HandlersMangerPtr = std::shared_ptr<HandlersManager>;