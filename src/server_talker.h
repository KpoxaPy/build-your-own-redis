#pragma once

#include "signal_slot.h"
#include "storage_middleware.h"
#include "talker.h"

#include "storage.h"
#include "events.h"
#include "server.h"

class ServerTalker : public Talker {
public:
  ServerTalker(EventLoopPtr event_loop);

  void listen(Message message) override;
  void interrupt() override;

  Message::Type expected() override;

  void set_server(ServerPtr);
  void set_storage(IStoragePtr);
  void set_replicas_manager(IReplicasManagerPtr);

private:
  ServerPtr _server;
  IStoragePtr _storage;
  IReplicasManagerPtr _replicas_manager;

  EventLoopPtr _event_loop;

  std::optional<ReplicaId> _replica_id;
  SlotPtr<Message> _slot_replica_command;
};