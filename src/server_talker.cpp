#include "server_talker.h"

#include "base64.h"
#include "storage.h"

const std::string dummy_rdb_file = "UkVESVMwMDEx+glyZWRpcy12ZXIFNy4yLjD6CnJlZGlzLWJpdHPAQPoFY3RpbWXCbQi8ZfoIdXNlZC1tZW3CsMQQAPoIYW9mLWJhc2XAAP/wbjv+wP9aog==";

ServerTalker::ServerTalker(EventLoopPtr event_loop)
  : _event_loop(event_loop) {
}

void ServerTalker::listen(Message message) {
  const bool is_replica = this->_server->is_replica();

  try {
    auto command = Command::try_parse(message);
    auto type = command->type();

    if (type == CommandType::Ping) {
      this->next_say(Message::Type::SimpleString, "PONG");

    } else if (type == CommandType::Echo) {
      auto& echo_command = dynamic_cast<EchoCommand&>(*command);

      this->next_say(Message::Type::BulkString, echo_command.data());

    } else if (type == CommandType::Set) {
      if (is_replica) {
        this->next_say(Message::Type::SimpleError, "cannot write: replica mode");
        return;
      }

      this->_storage_command.emit(command)
        .then([this](Message message) {
          this->next_say(std::move(message));
        });

    } else if (type == CommandType::Get) {
      this->_storage_command.emit(command)
        .then([this](Message message) {
          this->next_say(std::move(message));
        });

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

      this->next_say(Message::Type::BulkString, this->_server->info().to_string(info_parts));

    } else if (type == CommandType::ReplConf) {
      auto& cmd = static_cast<ReplConfCommand&>(*command);

      this->next_say(Message::Type::SimpleString, "OK");

    } else if (type == CommandType::Psync) {
      auto& cmd = static_cast<PsyncCommand&>(*command);

      std::ostringstream ss;
      ss << "FULLRESYNC"
        << " " << this->_server->info().replication.master_replid
        << " " << this->_server->info().replication.master_repl_offset;
      this->next_say(Message::Type::SimpleString, ss.str());
      this->next_say(Message::Type::SyncResponse, base64_decode(dummy_rdb_file));

    } else {
      this->next_say(Message::Type::SimpleError, "unimplemented command");
    }
  } catch (const CommandParseError& err) {
    this->next_say(Message::Type::SimpleError, err.what());
  }
}

Message::Type ServerTalker::expected() {
  return Message::Type::Any;
}

void ServerTalker::set_server(ServerPtr server) {
  this->_server = server;
}

void ServerTalker::connect_storage_command(SlotDescriptor<Message> descriptor) {
  this->_storage_command = std::move(descriptor);
}

void ServerTalker::connect_replica_add(SlotDescriptor<void> descriptor) {
  this->_replica_add = std::move(descriptor);
}
