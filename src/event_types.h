#pragma once

#include <cstddef>

using EventDescriptor = std::size_t;

enum : EventDescriptor {
  EventUndefined,

  EventPollerAdd,
  EventPollerRemove,

  EventLast,
};

class Event {
public:
  EventDescriptor descriptor() const;

protected:
  EventDescriptor _descriptor = EventUndefined;
};

class PollAddEvent : public Event {
public:
  PollAddEvent(int fd, short flags, EventDescriptor expected_descriptor);

  int fd;
  short flags;
  EventDescriptor expected_descriptor;
};

class PollRemoveEvent : public Event {
public:
  PollRemoveEvent(int fd);

  int fd;
};

class PollReadyEvent : public Event {
public:
  PollReadyEvent(EventDescriptor, int fd, short events);

  int fd;
  short events;
};