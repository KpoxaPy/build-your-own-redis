#include "server_talker.h"

#include "base64.h"
#include "utils.h"

#include <filesystem>
#include <fstream>

const std::string empty_rdb_file = "UkVESVMwMDEx+glyZWRpcy12ZXIFNy4yLjD6CnJlZGlzLWJpdHPAQPoFY3RpbWXCbQi8ZfoIdXNlZC1tZW3CsMQQAPoIYW9mLWJhc2XAAP/wbjv+wP9aog==";

ServerTalker::ServerTalker(EventLoopPtr event_loop)
  : _event_loop(event_loop)
{
  this->_slot_message = std::make_shared<Slot<Message>>([this](Message message) {
    this->next_say(std::move(message));
  });
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

      auto& set_command = dynamic_cast<SetCommand&>(*command);
      this->_storage->set(set_command.key(), set_command.value(), set_command.expire_ms());
      this->next_say(Message::Type::SimpleString, "OK");

    } else if (type == CommandType::Get) {
      auto& get_command = static_cast<GetCommand&>(*command);
      if (auto result = this->_storage->get(get_command.key())) {
        this->next_say(Message::Type::BulkString, result.value());
      } else {
        this->next_say(Message::Type::BulkString);
      }

    } else if (type == CommandType::Keys) {
      auto& keys_command = static_cast<KeysCommand&>(*command);
      auto keys = this->_storage->keys(keys_command.arg());

      std::vector<Message> array;
      for (auto& key : keys) {
        array.emplace_back(Message::Type::BulkString, std::move(key));
      }
      this->next_say(Message::Type::Array, std::move(array));

    } else if (type == CommandType::Config) {
      auto& config_command = static_cast<ConfigCommand&>(*command);

      auto action = to_lower_case(config_command.action());
      if (action == "get") {
        std::vector<Message> array;
        for (const auto& key : config_command.args()) {
          auto value = this->_server->info().get_config_value(key);
          if (value) {
            array.emplace_back(Message::Type::BulkString, key);
            array.emplace_back(Message::Type::BulkString, value.value());
          }
        }
        this->next_say(Message::Type::Array, std::move(array));
      } else {
        this->next_say(Message::Type::SimpleError, "unknown action for config command");
      }

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

      if (!this->_replica_id) {
        this->_replica_id = this->_replicas_manager->add_replica(this->_slot_message);
      }
      if (this->_replicas_manager->replica_process_conf(this->_replica_id.value(), command)) {
        this->next_say(Message::Type::SimpleString, "OK");
      }

    } else if (type == CommandType::Psync) {
      auto& cmd = static_cast<PsyncCommand&>(*command);

      std::ostringstream ss;
      ss << "FULLRESYNC"
        << " " << this->_server->info().replication.master_replid
        << " " << this->_server->info().replication.master_repl_offset;
      this->next_say(Message::Type::SimpleString, ss.str());

      if (std::filesystem::exists(this->_server->info().server.db_file_path())) {
        std::ifstream ifs(this->_server->info().server.db_file_path(), std::ios::binary);
        std::string contents(std::istreambuf_iterator<char>(ifs), {});
        this->next_say(Message::Type::SyncResponse, contents);
      } else {
        this->next_say(Message::Type::SyncResponse, base64_decode(empty_rdb_file));
      }

      this->_replicas_manager->replica_set_state(this->_replica_id.value(), IReplicasManager::ReplState::WRITE);

    } else if (type == CommandType::Wait) {
      auto& wait_command = static_cast<WaitCommand&>(*command);
      this->_replicas_manager->wait_for(wait_command.replicas(), wait_command.timeout_ms(), this->_slot_message);

    } else {
      this->next_say(Message::Type::SimpleError, "unimplemented command");
    }
  } catch (const CommandParseError& err) {
    this->next_say(Message::Type::SimpleError, err.what());
  }
}

void ServerTalker::interrupt() {
  if (this->_replica_id) {
    this->_replicas_manager->remove_replica(this->_replica_id.value());
    this->_replica_id.reset();
  }
}

Message::Type ServerTalker::expected() {
  return Message::Type::Any;
}

void ServerTalker::set_server(ServerPtr server) {
  this->_server = server;
}

void ServerTalker::set_storage(IStoragePtr storage) {
  this->_storage = std::move(storage);
}

void ServerTalker::set_replicas_manager(IReplicasManagerPtr replicas_manager) {
  this->_replicas_manager = std::move(replicas_manager);
}
