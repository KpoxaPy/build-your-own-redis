#pragma once

#include "events.h"
#include "signal_slot.h"

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

class Poller {
public:
  Poller(EventLoopPtr event_loop);

  SlotPtr<int, PollEventTypeList, SignalPtr<PollEventType>>& add_fd();
  SlotPtr<int>& remove_fd();

private:
  struct SocketEventHandler {
    int fd;
    short flags;
    SignalPtr<PollEventType> signal;
    std::size_t pos_in_fds;
  };

  SlotPtr<int, PollEventTypeList, SignalPtr<PollEventType>> _slot_add;
  SlotPtr<int> _slot_remove;
  EventLoopPtr _event_loop;

  EventLoop::JobHandle _start_handle;
  EventLoop::JobHandle _loop_handle;

  std::vector<pollfd> _fds;
  std::unordered_map<int, SocketEventHandler> _handlers;

  void start();
};
using PollerPtr = std::shared_ptr<Poller>;