#pragma once

#include <memory>
#include <functional>
#include <list>
#include <queue>

class EventLoopManager;
using EventLoopManagerPtr = std::shared_ptr<EventLoopManager>;

class EventLoopManager {
public:
  enum RunType {
    Once,
    Repeat,
  };

  using Func = std::function<void()>;

  static EventLoopManagerPtr make();

  void post(Func, RunType type = Once, std::string name = {});

  void start_loop();

private:
  std::list<Func> _repeated_events;
  std::queue<Func> _events;
};