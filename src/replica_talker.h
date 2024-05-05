#pragma once

#include "talker.h"
#include "server.h"

#include <string>

class ReplicaTalker : public Talker {
public:
  std::optional<Message> talk() override;
  std::optional<Message> talk(const Message&) override;

  Message::Type expected() override;

  void set_server(ServerPtr);

private:
  ServerPtr _server;

  int _state = 0;
};