#pragma once

#include "talker.h"

#include "server.h"
#include "storage.h"

class ServerTalker : public Talker {
public:
  std::optional<Message> talk() override;
  std::optional<Message> talk(const Message&) override;

  void set_storage(StoragePtr);
  void set_server(ServerPtr);

private:
  StoragePtr _storage;
  ServerPtr _server;
};