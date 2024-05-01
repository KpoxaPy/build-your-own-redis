#include "event_types.h"

EventDescriptor Event::descriptor() const {
  return this->_descriptor;
}

PollAddEvent::PollAddEvent(int fd, short flags, EventDescriptor expected_descriptor)
  : fd(fd)
  , flags(flags)
  , expected_descriptor(expected_descriptor)
{
  this->_descriptor = EventPollerAdd;
}

PollRemoveEvent::PollRemoveEvent(int fd)
  : fd(fd)
{
  this->_descriptor = EventPollerRemove;
}

PollReadyEvent::PollReadyEvent(EventDescriptor descriptor, int fd, short events) 
  : fd(fd)
  , events(events)
{
  this->_descriptor = descriptor;
}
