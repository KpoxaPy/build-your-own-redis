#pragma once

#include "events.h"

#include <memory>
#include <poll.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

enum class PollEventType {
  ReadyToRead,
  ReadyToWrite,
  HangUp,
  Error,
  InvalidFD,
};
using PollEventTypeList = std::unordered_set<PollEventType>;

class PollAddEvent : public Event {
public:
  PollAddEvent(int fd, PollEventTypeList types, EventDescriptor expected_descriptor);

  int fd;
  PollEventTypeList types;
  EventDescriptor expected_descriptor;
};

class PollRemoveEvent : public Event {
public:
  PollRemoveEvent(int fd);

  int fd;
};

class PollEvent : public Event {
public:
  PollEvent(int fd, PollEventType);

  int fd;
  PollEventType type;
};

class Poller {
public:
  Poller(EventLoopPtr event_loop);

  EventDescriptor add_listener();
  EventDescriptor remove_listener();
  void start();

private:
  struct SocketEventHandler {
    int fd;
    short flags;
    EventDescriptor event_descriptor;
    std::size_t pos_in_fds;
  };

  EventDescriptor _add_listener = UNDEFINED_EVENT;
  EventDescriptor _remove_listener = UNDEFINED_EVENT;
  EventLoopPtr _event_loop;

  std::vector<pollfd> _fds;
  std::unordered_map<int, SocketEventHandler> _handlers;
};
using PollerPtr = std::shared_ptr<Poller>;