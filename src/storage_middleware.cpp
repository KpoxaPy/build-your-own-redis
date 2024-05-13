#include "storage_middleware.h"

#include "command.h"
#include "utils.h"

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
  this->_replicas[id] = ReplicaHandle{
    .state = ReplState::MET,
    .slot_command = std::move(slot_command),
  };
  return id;
}

void StorageMiddleware::remove_replica(ReplicaId id) {
  this->_replicas.erase(id);
}

bool StorageMiddleware::replica_process_conf(ReplicaId id, CommandPtr command) {
  auto type = command->type();

  if (type == CommandType::ReplConf) {
    auto& replconf_command = dynamic_cast<ReplConfCommand&>(*command);

    const auto& argv = replconf_command.args();
    const auto argc = argv.size();
    if (argc != 2) {
      std::cerr << "REPLCONF expects exactly 2 arguments" << std::endl;
    }
    if (to_lower_case(argv[0]) == "ack") {
      return false;
    }
  }
  return true;
}

void StorageMiddleware::replica_set_state(ReplicaId id, ReplState state) {
  this->_replicas[id].state = state;
}

std::size_t StorageMiddleware::count_replicas() {
  return this->_replicas.size();
}

void StorageMiddleware::push(const Message & message) {
  for (const auto& [id, handle]: this->_replicas) {
    if (handle.state != ReplState::WRITE) {
      continue;
    }

    handle.slot_command->call(message);
  }
}
