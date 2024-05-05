#include "server_talker.h"

#include "command.h"

std::optional<Message> ServerTalker::talk() {
  return {};
}

std::optional<Message> ServerTalker::talk(const Message& message) {
  try {
    auto command = Command::try_parse(message);
    auto type = command->type();

    if (type == CommandType::Ping) {
      return Message(Message::Type::SimpleString, {"PONG"});

    } else if (type == CommandType::Echo) {
      auto& echo_command = dynamic_cast<EchoCommand&>(*command);

      return Message(Message::Type::BulkString, echo_command.data());

    } else if (type == CommandType::Set) {
      auto& set_command = dynamic_cast<SetCommand&>(*command);

      this->_storage->storage[set_command.key()] = set_command.value();
      Value& stored_value = this->_storage->storage[set_command.key()];

      if (set_command.expire_ms()) {
        stored_value.setExpire(std::chrono::milliseconds{set_command.expire_ms().value()});
      }

      return Message(Message::Type::SimpleString, "OK");

    } else if (type == CommandType::Get) {
      auto& get_command = static_cast<GetCommand&>(*command);

      auto it = this->_storage->storage.find(get_command.key());
      if (it == this->_storage->storage.end()) {
        return Message(Message::Type::BulkString, {});
      }
      auto& value = it->second;

      if (value.getExpire() && Clock::now() >= value.getExpire()) {
        this->_storage->storage.erase(get_command.key());
        return Message(Message::Type::BulkString, {});
      }

      return Message(Message::Type::BulkString, value.data());

    } else if (type == CommandType::Info) {
      auto& info_command = static_cast<InfoCommand&>(*command);

      std::unordered_set<std::string> info_parts;

      auto default_parts = [&info_parts]() {
        info_parts.insert("server");
        info_parts.insert("replication");
      };

      for (const auto& info_part : info_command.args()) {
        if (info_part == "default") {
          default_parts();
        } else {
          info_parts.insert(info_part);
        }
      }

      if (info_parts.size() == 0) {
        default_parts();
      }

      return Message(Message::Type::BulkString, this->_server->info().to_string(info_parts));

    } else if (type == CommandType::ReplConf) {
      auto& cmd = static_cast<ReplConfCommand&>(*command);

      return Message(Message::Type::SimpleString, {"OK"});

    } else if (type == CommandType::Psync) {
      auto& cmd = static_cast<PsyncCommand&>(*command);

      std::ostringstream ss;
      ss << "FULLRESYNC"
        << " " << this->_server->info().replication.master_replid
        << " " << this->_server->info().replication.master_repl_offset;
      return Message(Message::Type::SimpleString, ss.str());

    } else {
      return Message(Message::Type::SimpleError, {"unimplemented command"});
    }
  } catch (const CommandParseError& err) {
    return Message(Message::Type::SimpleError, {err.what()});
  }
}

void ServerTalker::set_storage(StoragePtr storage) {
  this->_storage = storage;
}

void ServerTalker::set_server(ServerPtr server) {
  this->_server = server;
}
