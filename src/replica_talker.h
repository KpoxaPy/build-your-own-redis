#pragma once

#include "talker.h"
#include "server.h"

#include <string>

class ReplicaTalker : public Talker {
public:
  ReplicaTalker();

  void listen(Message) override;

  Message::Type expected() override;

  void set_server(ServerPtr);

private:
  ServerPtr _server;

  int _state = 0;
};
