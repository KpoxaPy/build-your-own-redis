#pragma once

#include "events.h"
#include "handler.h"
#include "poller.h"
#include "signal_slot.h"
#include "talker.h"

#include <memory>
#include <unordered_map>

class HandlersManager {
public:
  using TalkerBuilder = std::function<TalkerPtr()>;

  HandlersManager(EventLoopPtr event_loop);

  void set_talker(TalkerBuilder);

  SlotPtr<int>& add_fd();
  SlotPtr<int>& remove_fd();
  SignalPtr<int, PollEventTypeList, SignalPtr<PollEventType>>& new_fd();
  SignalPtr<int>& removed_fd();

private:
  SlotPtr<int> _slot_add;
  SlotPtr<int> _slot_remove;
  SignalPtr<int, PollEventTypeList, SignalPtr<PollEventType>> _new_fd_signal;
  SignalPtr<int> _removed_fd_signal;

  EventLoopPtr _event_loop;

  TalkerBuilder _talker_builder;

  std::unordered_map<int, Handler> _handlers;

  void add(int fd);
  void remove(int fd);
};
using HandlersMangerPtr = std::shared_ptr<HandlersManager>;