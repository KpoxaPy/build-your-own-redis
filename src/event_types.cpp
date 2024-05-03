#include "event_types.h"

EventDescriptor Event::descriptor() const {
  return this->_descriptor;
}

EventPtr PollAddEvent::make(int fd, std::unordered_set<PollEventType> types, EventDescriptor expected_descriptor) {
  return std::make_unique<PollAddEvent>(fd, std::move(types), expected_descriptor);
}

PollAddEvent::PollAddEvent(int fd, std::unordered_set<PollEventType> types, EventDescriptor expected_descriptor)
    : fd(fd), types(types), expected_descriptor(expected_descriptor) {
  this->_descriptor = EventPollerAdd;
}

EventPtr PollRemoveEvent::make(int fd) {
  return std::make_unique<PollRemoveEvent>(fd);
}

PollRemoveEvent::PollRemoveEvent(int fd)
    : fd(fd) {
  this->_descriptor = EventPollerRemove;
}

EventPtr PollEvent::make(EventDescriptor descriptor, int fd, PollEventType type) {
  return std::make_unique<PollEvent>(descriptor, fd, type);
}

PollEvent::PollEvent(EventDescriptor descriptor, int fd, PollEventType type)
    : fd(fd), type(type) {
  this->_descriptor = descriptor;
}

EventPtr HandlerAddEvent::make(int fd) {
  return std::make_unique<HandlerAddEvent>(fd);
}

HandlerAddEvent::HandlerAddEvent(int fd)
  : fd(fd) {
  this->_descriptor = EventHandlerAdd;
}

EventPtr HandlerRemoveEvent::make(int fd) {
  return std::make_unique<HandlerRemoveEvent>(fd);
}

HandlerRemoveEvent::HandlerRemoveEvent(int fd)
  : fd(fd) {
  this->_descriptor = EventHandlerRemove;
}
