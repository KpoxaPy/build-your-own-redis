#include "storage_middleware.h"

#include "command.h"
#include "debug.h"
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
  this->_replicas.try_emplace(id, *this, id, ReplState::MET, std::move(slot_message));
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
  for (auto& [id, handle]: this->_replicas) {
    handle.push(message);
  }
}

StorageMiddleware::ReplicaHandle::ReplicaHandle(StorageMiddleware& parent, ReplicaId id, ReplState state, SlotPtr<Message> slot_message)
  : parent(parent), id(id), state(state), slot_message(slot_message) {
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

        for (auto it = this->parent._waits.begin(); it != this->parent._waits.end();) {
          auto& wait = *it++;
          wait->update_replica_ack(*this);
        }
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

  this->bytes_pushed += message.size();

  this->slot_message->call(message);
}

StorageMiddleware::WaitHandle::WaitHandle(
  StorageMiddleware& parent,
  std::size_t count,
  std::size_t timeout_ms,
  SlotPtr<Message> slot_message)
    : parent(parent)
    , replicas_expected(count)
    , timeout_ms(timeout_ms)
    , slot_message(slot_message) {
  if (DEBUG_LEVEL >= 1) {
    std::cerr << "DEBUG Wait add" << std::endl;
    std::cerr << "  replicas_expected = " << this->replicas_expected << std::endl;
    std::cerr << "  timeout_ms = " << this->timeout_ms << std::endl;
  }
}

void StorageMiddleware::WaitHandle::update_replica_ack(ReplicaHandle& replica) {
  auto it = this->replica_ack_waitlist.find(replica.id);
  if (it == this->replica_ack_waitlist.end()) {
    return;
  }

  auto expected_bytes = it->second;
  this->replica_ack_waitlist.erase(it);

  if (DEBUG_LEVEL >= 1) {
    std::cerr << "DEBUG Wait recieved ack from repl #" << replica.id
      << " ack " << replica.bytes_ack << " bytes,"
      << " expected " << expected_bytes << " bytes" << std::endl;
  }

  if (replica.bytes_ack < expected_bytes) {
    return;
  }

  ++this->replicas_ready;

  if (this->replicas_expected >= this->replicas_ready) {
    this->reply();
  }
}

void StorageMiddleware::WaitHandle::setup() {
  this->replicas_ready = 0;
  for (auto& [id, handle] : this->parent._replicas) {
    if (handle.bytes_ack >= handle.bytes_pushed) {
      ++this->replicas_ready;
    } else {
      this->replica_ack_waitlist[id] = handle.bytes_pushed;
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

  for (auto& [id, bytes_expected] : this->replica_ack_waitlist) {
    if (DEBUG_LEVEL >= 1) {
      std::cerr << "DEBUG Send getack to repl #" << id << ", expected " << bytes_expected << " bytes" << std::endl;
    }
    this->parent._replicas.at(id).push(ReplConfCommand("GETACK", "*").construct());
  }

  this->timeout = this->parent._event_loop->set_timeout(this->timeout_ms, [this]() {
    this->reply();
  });
}

void StorageMiddleware::WaitHandle::reply() {
  if (DEBUG_LEVEL >= 1) {
    std::cerr << "DEBUG Wait reply" << std::endl;
    std::cerr << "  replicas_ready = " << this->replicas_ready << std::endl;
  }
  this->slot_message->call(Message(Message::Type::Integer, static_cast<int>(this->replicas_ready)));
  this->timeout.invalidate();
  parent._waits.erase(this->it);
}
