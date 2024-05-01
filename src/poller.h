#pragma once

#include "events.h"

#include <memory>
#include <poll.h>
#include <unordered_map>
#include <vector>

class Poller;
using PollerPtr = std::shared_ptr<Poller>;

class Poller {
public:
  static PollerPtr make();

  void start(EventLoopManagerPtr event_loop);

private:
  struct SocketEventHandler {
    int fd;
    short flags;
    EventDescriptor event_descriptor;
    std::size_t pos_in_fds;
  };

  EventLoopManagerPtr _event_loop;

  std::vector<pollfd> _fds;
  std::unordered_map<int, SocketEventHandler> _handlers;
};