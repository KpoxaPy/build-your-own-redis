#include "poller.h"

PollerPtr Poller::make() {
  return std::make_shared<Poller>();
}

void Poller::start(EventLoopManagerPtr event_loop) {
}
