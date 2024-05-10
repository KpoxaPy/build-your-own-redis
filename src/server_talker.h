#pragma once

#include "talker.h"

#include "command.h"
#include "events.h"
#include "server.h"

class ServerTalker : public Talker {
public:
  ServerTalker(EventLoopPtr event_loop);

  void listen(Message message) override;

  Message::Type expected() override;

  void set_server(ServerPtr);

  void connect_storage_command(SlotDescriptor<Message>);
  void connect_replica_add(SlotDescriptor<void>);

private:
  SlotDescriptor<Message> _storage_command;
  SlotDescriptor<void> _replica_add;
  EventLoopPtr _event_loop;

  ServerPtr _server;
};