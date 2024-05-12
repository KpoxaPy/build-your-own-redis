#pragma once

#include "server.h"
#include "storage.h"
#include "talker.h"

#include <string>

class ReplicaTalker : public Talker {
public:
  ReplicaTalker();

  void listen(Message) override;

  Message::Type expected() override;

  void set_server(ServerPtr);
  void set_storage(IStoragePtr);

private:
  ServerPtr _server;
  IStoragePtr _storage;

  int _state = 0;
};
