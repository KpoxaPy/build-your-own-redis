#pragma once

#include "events.h"
#include "handler.h"
#include "talker.h"

#include <memory>

class HandlersManager {
public:
  using TalkerBuilder = std::function<TalkerPtr()>;

  HandlersManager(EventLoopPtr event_loop);

  void set_talker(TalkerBuilder);

  void connect_poller_add(SlotDescriptor<void>);
  void connect_poller_remove(SlotDescriptor<void>);
  SlotDescriptor<void> add();
  SlotDescriptor<void> remove();

private:
  SlotDescriptor<void> _poller_add;
  SlotDescriptor<void> _poller_remove;

  SlotHolderPtr<void> _slot_add;
  SlotHolderPtr<void> _slot_remove;

  EventLoopPtr _event_loop;

  TalkerBuilder _talker_builder;

  std::unordered_map<int, Handler> _handlers;

  void add(int fd);
  void remove(int fd);
};
using HandlersMangerPtr = std::shared_ptr<HandlersManager>;