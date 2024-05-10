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

class Poller {
public:
  Poller(EventLoopPtr event_loop);

  SlotDescriptor<void> add();
  SlotDescriptor<void> remove();

private:
  struct SocketEventHandler {
    int fd;
    short flags;
    SlotDescriptor<void> slot;
    std::size_t pos_in_fds;
  };

  SlotHolderPtr<void> _slot_add;
  SlotHolderPtr<void> _slot_remove;
  EventLoopPtr _event_loop;

  std::vector<pollfd> _fds;
  std::unordered_map<int, SocketEventHandler> _handlers;

  void start();
};
using PollerPtr = std::shared_ptr<Poller>;