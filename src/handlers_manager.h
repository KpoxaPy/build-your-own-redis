#pragma once

#include "events.h"
#include "handler.h"
#include "server.h"
#include "storage.h"

#include <memory>

class HandlersManager;
using HandlersMangerPtr = std::shared_ptr<HandlersManager>;
class HandlersManager {
public:
  static HandlersMangerPtr make();

  void set_server(ServerPtr);
  void set_storage(StoragePtr);

  void start(EventLoopManagerPtr event_loop);

  ServerPtr server();
  StoragePtr storage();

private:
  EventLoopManagerPtr _event_loop;
  ServerPtr _server;
  StoragePtr _storage;

  std::unordered_map<int, Handler> _handlers;

  void add(int fd);
  void remove(int fd);
};