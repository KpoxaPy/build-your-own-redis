#include "poller.h"

#include "debug.h"

#include <iostream>
#include <sstream>

PollAddEvent::PollAddEvent(int fd, PollEventTypeList types, EventDescriptor expected_descriptor)
    : fd(fd), types(types), expected_descriptor(expected_descriptor) {
}

PollRemoveEvent::PollRemoveEvent(int fd)
    : fd(fd) {
}

PollEvent::PollEvent(int fd, PollEventType type)
    : fd(fd), type(type) {
}


Poller::Poller(EventLoopPtr event_loop)
  : _event_loop(event_loop) {
  this->_add_listener = this->_event_loop->make_desciptor();
  this->_remove_listener = this->_event_loop->make_desciptor();
}

EventDescriptor Poller::add_listener() {
  return this->_add_listener;
}

EventDescriptor Poller::remove_listener() {
  return this->_remove_listener;
}

void Poller::start() {
  this->_event_loop->listen(this->_add_listener, [this](const Event& event) {
    auto& add_event = static_cast<const PollAddEvent&>(event);

    short flags;
    if (add_event.types.contains(PollEventType::ReadyToRead)) {
      flags |= POLLIN;
    }
    if (add_event.types.contains(PollEventType::ReadyToWrite)) {
      flags |= POLLOUT;
    }

    auto it = this->_handlers.find(add_event.fd);
    if (it == this->_handlers.end()) {
      this->_fds.push_back(pollfd{
        .fd = add_event.fd,
        .events = flags
      });

      this->_handlers[add_event.fd] = SocketEventHandler{
        .fd = add_event.fd,
        .flags = flags,
        .event_descriptor = add_event.expected_descriptor,
        .pos_in_fds = this->_fds.size() - 1
      };
    } else {
      auto& handler = it->second;
      handler.flags = flags;
      handler.event_descriptor = add_event.expected_descriptor;
      this->_fds[handler.pos_in_fds].events = flags;
    }
  });

  this->_event_loop->listen(this->_remove_listener, [this](const Event& event) {
    auto& remove_event = static_cast<const PollRemoveEvent&>(event);

    auto it = this->_handlers.find(remove_event.fd);
    if (it == this->_handlers.end()) {
      return;
    }

    auto& handler = it->second;
    this->_fds[handler.pos_in_fds].fd = -1; // ignore this socket in polling
    // TODO make cleanup process, running periodically
    this->_handlers.erase(remove_event.fd);
  });

  this->_event_loop->repeat([this]() {
    if (this->_handlers.size() == 0) {
      return;
    }

    auto poll_res = ::poll(this->_fds.data(), this->_fds.size(), 0);

    if (poll_res < 0) {
      std::ostringstream ss;
      ss << "Poller got error on poll: errno=" << errno;
      throw std::runtime_error(ss.str());
    }

    if (poll_res == 0) {
      return;
    }

    if (DEBUG_LEVEL >= 2) std::cerr << "DEBUG Poller fd with events count = " << poll_res << std::endl;

    for (const auto& fd: this->_fds) {
      if (fd.revents > 0) {
        --poll_res;

        auto& handler = this->_handlers[fd.fd];

        if (fd.revents & POLLNVAL) {
          this->_event_loop->post<PollEvent>(handler.event_descriptor, handler.fd, PollEventType::InvalidFD);
          if (DEBUG_LEVEL >= 2) std::cerr << "DEBUG InvalidFD event sent to handler with fd = " << handler.fd << std::endl;
        }

        if (fd.revents & POLLERR) {
          this->_event_loop->post<PollEvent>(handler.event_descriptor, handler.fd, PollEventType::Error);
          if (DEBUG_LEVEL >= 2) std::cerr << "DEBUG Error event sent to handler with fd = " << handler.fd << std::endl;
        }

        if (fd.revents & POLLHUP) {
          this->_event_loop->post<PollEvent>(handler.event_descriptor, handler.fd, PollEventType::HangUp);
          if (DEBUG_LEVEL >= 2) std::cerr << "DEBUG HangUp event sent to handler with fd = " << handler.fd << std::endl;
        }

        if (handler.flags & POLLIN && fd.revents & POLLIN) {
          this->_event_loop->post<PollEvent>(handler.event_descriptor, handler.fd, PollEventType::ReadyToRead);
          if (DEBUG_LEVEL >= 2) std::cerr << "DEBUG ReadyToRead event sent to handler with fd = " << handler.fd << std::endl;
        }

        if (handler.flags & POLLOUT && fd.revents & POLLOUT) {
          this->_event_loop->post<PollEvent>(handler.event_descriptor, handler.fd, PollEventType::ReadyToWrite);
          if (DEBUG_LEVEL >= 2) std::cerr << "DEBUG ReadyToWrite event sent to handler with fd = " << handler.fd << std::endl;
        }
      }

      if (poll_res == 0) {
        break;
      }
    }
  });
}
