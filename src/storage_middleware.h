#pragma once

#include "message.h"
#include "events.h"
#include "signal_slot.h"
#include "storage.h"

#include <memory>
#include <unordered_map>

using ReplicaId = std::size_t;

class IReplicasManager {
public:
  virtual ~IReplicasManager() = default;
  virtual ReplicaId add_replica(SlotPtr<Message> slot_command) = 0;
  virtual void remove_replica(ReplicaId) = 0;
};
using IReplicasManagerPtr = std::shared_ptr<IReplicasManager>;

class StorageMiddleware : public IStorage, public IReplicasManager {
  struct ReplicaHandle {
    SlotPtr<Message> slot_command;
  };

public:
  StorageMiddleware(EventLoopPtr event_loop);

  void set_storage(IStoragePtr);

  void set(std::string key, std::string value, std::optional<int> expire_ms) override;
  std::optional<std::string> get(std::string key) override;

  ReplicaId add_replica(SlotPtr<Message> slot_command) override;
  void remove_replica(ReplicaId) override;

private:
  EventLoopPtr _event_loop;

  IStoragePtr _storage;

  ReplicaId _next_replica_id = 0;
  std::unordered_map<ReplicaId, ReplicaHandle> _replicas;

  void push(const Message&);
};