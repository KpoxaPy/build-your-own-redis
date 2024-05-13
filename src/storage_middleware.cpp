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

ReplicaId StorageMiddleware::add_replica(SlotPtr<Message> slot_message) {
  auto id = this->_next_replica_id++;
  this->_replicas[id] = ReplicaHandle{
    .state = ReplState::MET,
    .slot_message = std::move(slot_message),
  };
  return id;
}

void StorageMiddleware::remove_replica(ReplicaId id) {
  this->_replicas.erase(id);
}

bool StorageMiddleware::replica_process_conf(ReplicaId id, CommandPtr command) {
  auto it = this->_replicas.find(id);
  if (it != this->_replicas.end()) {
    return it->second.process_conf(command);
  }
  return false;
}

void StorageMiddleware::replica_set_state(ReplicaId id, ReplState state) {
  auto it = this->_replicas.find(id);
  if (it != this->_replicas.end()) {
    it->second.set_state(state);
  }
}

std::size_t StorageMiddleware::count_replicas() {
  return this->_replicas.size();
}

void StorageMiddleware::wait_for(std::size_t count, std::size_t timeout_ms, SlotPtr<Message> slot_message) {
  auto wait_handle_ptr = std::make_shared<WaitHandle>(*this, count, timeout_ms, slot_message);
  wait_handle_ptr->it = this->_waits.insert(this->_waits.end(), wait_handle_ptr);
  wait_handle_ptr->setup();
}

void StorageMiddleware::push(const Message & message) {
  this->_bytes_pushed += message.size();
  for (auto& [id, handle]: this->_replicas) {
    handle.push(message);
  }
}

bool StorageMiddleware::ReplicaHandle::process_conf(CommandPtr command) {
  auto type = command->type();

  if (type == CommandType::ReplConf) {
    auto& replconf_command = dynamic_cast<ReplConfCommand&>(*command);

    const auto& argv = replconf_command.args();
    const auto argc = argv.size();
    if (argc != 2) {
      std::cerr << "REPLCONF expects exactly 2 arguments" << std::endl;
    }
    if (to_lower_case(argv[0]) == "ack") {
      auto maybe_bytes_ack = parseInt(argv[1].data(), argv[1].size());
      if (maybe_bytes_ack) {
        this->bytes_ack = maybe_bytes_ack.value();
      }

      return false;
    }
  }
  return true;
}

void StorageMiddleware::ReplicaHandle::set_state(ReplState state) {
  this->state = state;
}

void StorageMiddleware::ReplicaHandle::push(const Message& message) {
  if (this->state != ReplState::WRITE) {
    return;
  }

  this->slot_message->call(message);
}

StorageMiddleware::WaitHandle::WaitHandle(
  StorageMiddleware& parent,
  std::size_t count,
  std::size_t timeout_ms,
  SlotPtr<Message> slot_message)
    : parent(parent)
    , bytes_expected(parent._bytes_pushed)
    , replicas_expected(count)
    , timeout_ms(timeout_ms)
    , slot_message(slot_message) {
}

void StorageMiddleware::WaitHandle::setup() {
  this->replicas_ready = 0;
  for (auto& [id, handle] : this->parent._replicas) {
    if (handle.bytes_ack >= this->bytes_expected) {
      this->replicas_ready++;
    }
  }

  if (this->replicas_expected <= this->replicas_ready) {
    this->reply();
    return;
  }

  if (this->replicas_ready == this->parent._replicas.size()) {
    this->reply();
    return;
  }

  for (auto& [id, handle] : this->parent._replicas) {
    if (handle.bytes_ack < this->bytes_expected) {
      // TODO send getack
    }
  }

  this->timeout = this->parent._event_loop->set_timeout(this->timeout_ms, [this]() {
    this->reply();
  });
}

void StorageMiddleware::WaitHandle::reply() {
  this->slot_message->call(Message(Message::Type::Integer, static_cast<int>(this->replicas_ready)));
  this->timeout.invalidate();
  parent._waits.erase(this->it);
}
