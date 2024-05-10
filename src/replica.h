#pragma once

#include "events.h"
#include "handler.h"
#include "replica_talker.h"
#include "server.h"
#include "storage.h"

#include <memory>
#include <optional>

class Replica {
public:
  Replica(EventLoopPtr);

  void set_storage(StoragePtr);
  void set_server(ServerPtr);

  void connect_poller_add(SlotDescriptor<void>);
  void connect_poller_remove(SlotDescriptor<void>);

private:
  StoragePtr _storage;
  ServerPtr _server;

  SlotDescriptor<void> _poller_add;
  SlotDescriptor<void> _poller_remove;
  EventLoopPtr _event_loop;

  std::optional<int> _master_fd;
  std::unique_ptr<Handler> _handler;
  std::shared_ptr<ReplicaTalker> _talker;

  void start();
};
using ReplicaPtr = std::shared_ptr<Replica>;
