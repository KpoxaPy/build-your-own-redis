#pragma once

#include "talker.h"

#include "server.h"
#include "storage.h"

#include <deque>

class ServerTalker : public Talker {
public:
  void listen(Message message) override;

  Message::Type expected() override;

  void set_storage(StoragePtr);
  void set_server(ServerPtr);

private:
  StoragePtr _storage;
  ServerPtr _server;
};