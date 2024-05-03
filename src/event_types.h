#pragma once

#include <cstddef>
#include <memory>
#include <unordered_set>

using EventDescriptor = std::size_t;

enum : EventDescriptor {
  EventUndefined,

  EventPollerAdd,
  EventPollerRemove,

  EventHandlerAdd,
  EventHandlerRemove,

  EventLast,
};

class Event {
public:
  EventDescriptor descriptor() const;

protected:
  EventDescriptor _descriptor = EventUndefined;
};
using EventPtr = std::unique_ptr<Event>;

enum class PollEventType {
  ReadyToRead,
  ReadyToWrite,
  HangUp,
  Error,
  InvalidFD,
};

class PollAddEvent : public Event {
public:
  static EventPtr make(int fd, std::unordered_set<PollEventType> types, EventDescriptor expected_descriptor);

  PollAddEvent(int fd, std::unordered_set<PollEventType> types, EventDescriptor expected_descriptor);

  int fd;
  std::unordered_set<PollEventType> types;
  EventDescriptor expected_descriptor;
};

class PollRemoveEvent : public Event {
public:
  static EventPtr make(int fd);

  PollRemoveEvent(int fd);

  int fd;
};

class PollEvent : public Event {
public:
  static EventPtr make(EventDescriptor, int fd, PollEventType);

  PollEvent(EventDescriptor, int fd, PollEventType);

  int fd;
  PollEventType type;
};

class HandlerAddEvent : public Event {
public:
  static EventPtr make(int fd);

  HandlerAddEvent(int fd);

  int fd;
};

class HandlerRemoveEvent : public Event {
public:
  static EventPtr make(int fd);

  HandlerRemoveEvent(int fd);

  int fd;
};
