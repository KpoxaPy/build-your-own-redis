#pragma once

#include "events.h"

#include <memory>

class Poller;
using PollerPtr = std::shared_ptr<Poller>;

class Poller {
public:
  static PollerPtr make();

  void start(EventLoopManagerPtr event_loop);

};