#pragma once

#include "signal_slot.h"
#include "handler.h"
#include "replica_talker.h"
#include "server.h"
#include "storage.h"

#include <memory>
#include <optional>

class Replica {
public:
  Replica(EventLoopPtr);

  void set_server(ServerPtr);
  void set_storage(IStoragePtr);

  SignalPtr<int, PollEventTypeList, SignalPtr<PollEventType>>& new_fd();
  SignalPtr<int>& removed_fd();

private:
  IStoragePtr _storage;
  ServerPtr _server;

  SignalPtr<int, PollEventTypeList, SignalPtr<PollEventType>> _new_fd_signal;
  SignalPtr<int> _removed_fd_signal;

  EventLoopPtr _event_loop;

  std::optional<int> _master_fd;
  std::unique_ptr<Handler> _handler;
  std::shared_ptr<ReplicaTalker> _talker;

  void start();
};
using ReplicaPtr = std::shared_ptr<Replica>;
