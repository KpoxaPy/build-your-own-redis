#pragma once

#include "events.h"
#include "server.h"

#include <memory>

class Replica;
using ReplicaPtr = std::shared_ptr<Replica>;

class Replica {
public:
  static ReplicaPtr make();

  void set_storage(StoragePtr);
  void set_server(ServerPtr);

  void start(EventLoopManagerPtr);

private:
  StoragePtr _storage;
  ServerPtr _server;

  EventLoopManagerPtr _event_loop;
};
