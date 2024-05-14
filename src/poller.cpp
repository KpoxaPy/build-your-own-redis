#include "poller.h"

#include "debug.h"

#include <iostream>
#include <sstream>

Poller::Poller(EventLoopPtr event_loop)
  : _event_loop(event_loop) {
  this->_slot_add = std::make_shared<Slot<int, PollEventTypeList, SignalPtr<PollEventType>>>(
    [this](int fd, PollEventTypeList types, SignalPtr<PollEventType> signal) {
      short flags;
      if (types.contains(PollEventType::ReadyToRead)) {
        flags |= POLLIN;
      }
      if (types.contains(PollEventType::ReadyToWrite)) {
        flags |= POLLOUT;
      }

      auto it = this->_handlers.find(fd);
      if (it == this->_handlers.end()) {
        this->_fds.push_back(pollfd{
            .fd = fd,
            .events = flags});

        this->_handlers[fd] = SocketEventHandler{
            .fd = fd,
            .flags = flags,
            .signal = signal,
            .pos_in_fds = this->_fds.size() - 1};
      } else {
        auto& handler = it->second;
        handler.flags = flags;
        handler.signal = signal;
        this->_fds[handler.pos_in_fds].events = flags;
      }
    });

  this->_slot_remove = std::make_shared<Slot<int>>([this](int fd) {
    auto it = this->_handlers.find(fd);
    if (it == this->_handlers.end()) {
      return;
    }

    auto& handler = it->second;
    this->_fds[handler.pos_in_fds].fd = -1; // ignore this socket in polling
    // TODO make cleanup process, running periodically
    this->_handlers.erase(fd);
  });

  this->_start_handle = this->_event_loop->post([this](){
    this->start();
  });
}

SlotPtr<int, PollEventTypeList, SignalPtr<PollEventType>>& Poller::add_fd() {
  return this->_slot_add;
}

SlotPtr<int>& Poller::remove_fd() {
  return this->_slot_remove;
}

void Poller::start() {
  this->_loop_handle = this->_event_loop->repeat([this]() {
    if (this->_handlers.size() == 0) {
      return;
    }

    auto poll_res = ::poll(this->_fds.data(), this->_fds.size(), 1);

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
          handler.signal->emit(PollEventType::InvalidFD);
          if (DEBUG_LEVEL >= 2) std::cerr << "DEBUG InvalidFD event sent to handler with fd = " << handler.fd << std::endl;
        }

        if (fd.revents & POLLERR) {
          handler.signal->emit(PollEventType::Error);
          if (DEBUG_LEVEL >= 2) std::cerr << "DEBUG Error event sent to handler with fd = " << handler.fd << std::endl;
        }

        if (fd.revents & POLLHUP) {
          handler.signal->emit(PollEventType::HangUp);
          if (DEBUG_LEVEL >= 2) std::cerr << "DEBUG HangUp event sent to handler with fd = " << handler.fd << std::endl;
        }

        if (handler.flags & POLLIN && fd.revents & POLLIN) {
          handler.signal->emit(PollEventType::ReadyToRead);
          if (DEBUG_LEVEL >= 2) std::cerr << "DEBUG ReadyToRead event sent to handler with fd = " << handler.fd << std::endl;
        }

        if (handler.flags & POLLOUT && fd.revents & POLLOUT) {
          handler.signal->emit(PollEventType::ReadyToWrite);
          if (DEBUG_LEVEL >= 2) std::cerr << "DEBUG ReadyToWrite event sent to handler with fd = " << handler.fd << std::endl;
        }
      }

      if (poll_res == 0) {
        break;
      }
    }
  });
}
