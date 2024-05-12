#include "storage_middleware.h"

#include "command.h"

StorageMiddleware::StorageMiddleware(EventLoopPtr event_loop)
  : _event_loop(event_loop) {
}

void StorageMiddleware::set_storage(IStoragePtr storage) {
  this->_storage = storage;
}

void StorageMiddleware::set(std::string key, std::string value, std::optional<int> expire_ms) {
  this->_storage->set(key, value, expire_ms);

  SetCommand command(key, value, expire_ms);
  this->push(command.construct());
}

std::optional<std::string> StorageMiddleware::get(std::string key) {
  return this->_storage->get(std::move(key));
}

ReplicaId StorageMiddleware::add_replica(SlotPtr<Message> slot_command) {
  auto id = this->_next_replica_id++;
  this->_replicas[id] = {std::move(slot_command)};
  return id;
}

void StorageMiddleware::remove_replica(ReplicaId id) {
  this->_replicas.erase(id);
}

void StorageMiddleware::push(const Message & message) {
  for (const auto& [id, handle]: this->_replicas) {
    handle.slot_command->call(message);
  }
}
