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

  void connect_poller_add(EventDescriptor);
  void connect_poller_remove(EventDescriptor);
  void start();

private:
  StoragePtr _storage;
  ServerPtr _server;

  EventDescriptor _poller_add = UNDEFINED_EVENT;
  EventDescriptor _poller_remove = UNDEFINED_EVENT;
  EventLoopPtr _event_loop;

  std::optional<int> _master_fd;
  std::unique_ptr<Handler> _handler;
  std::shared_ptr<ReplicaTalker> _talker;
};
using ReplicaPtr = std::shared_ptr<Replica>;
