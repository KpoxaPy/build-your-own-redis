#pragma once

#include "events.h"
#include "storage.h"

#include <memory>

class IReplicasManager {
public:
  virtual ~IReplicasManager() = default;
  virtual void add_replica() = 0;
  virtual void remove_replica() = 0;
};
using IReplicasManagerPtr = std::shared_ptr<IReplicasManager>;

class StorageMiddleware : public IStorage, public IReplicasManager {
public:
  StorageMiddleware(EventLoopPtr event_loop);

  void set_storage(IStoragePtr);

  void set(std::string key, std::string value, std::optional<int> expire_ms) override;
  std::optional<std::string> get(std::string key) override;

  void add_replica() override;
  void remove_replica() override;

private:
  EventLoopPtr _event_loop;

  IStoragePtr _storage;
};