#pragma once

#include "events.h"
#include "server.h"
#include "storage.h"

#include <memory>

class Replica {
public:
  Replica(EventLoopPtr);

  void set_storage(StoragePtr);
  void set_server(ServerPtr);

  void start();

private:
  StoragePtr _storage;
  ServerPtr _server;

  EventLoopPtr _event_loop;
};
using ReplicaPtr = std::shared_ptr<Replica>;
