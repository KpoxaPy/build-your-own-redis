#include "replica_talker.h"

#include "command.h"
#include "utils.h"

enum : int {
  INIT = 0,
  UNDEFINED = 1,
  WAIT_FIRST_PONG = 2,
  WAIT_OK_FOR_REPLCONF_PORT = 3,
  WAIT_OK_FOR_REPLCONF_CAPA = 4,
  WAIT_FOR_PSYNC_ANSWER = 5,
  WAIT_FOR_RDB_FILE_SYNC = 6,
  WAIT_SERVER_COMMANDS = 7,
};

ReplicaTalker::ReplicaTalker() {
  this->_state = WAIT_FIRST_PONG;
  this->_pending.push_back(PingCommand().construct());
}

void ReplicaTalker::listen(Message message) {
  if (this->_state == WAIT_FIRST_PONG) {
    if (message.type() == Message::Type::SimpleString) {
      auto str = to_lower_case(get<std::string>(message.getValue()));

      if (str == "pong") {
        this->_state = WAIT_OK_FOR_REPLCONF_PORT;
        this->next_say<ReplConfCommand>("listening-port", std::to_string(this->_server->info().server.tcp_port));
      }
    }
  } else if (this->_state == WAIT_OK_FOR_REPLCONF_PORT) {
    if (message.type() == Message::Type::SimpleString) {
      auto str = to_lower_case(get<std::string>(message.getValue()));

      if (str == "ok") {
        this->_state = WAIT_OK_FOR_REPLCONF_CAPA;
        this->next_say<ReplConfCommand>("capa", "psync2");
      }
    }
  } else if (this->_state == WAIT_OK_FOR_REPLCONF_CAPA) {
    if (message.type() == Message::Type::SimpleString) {
      auto str = to_lower_case(get<std::string>(message.getValue()));

      if (str == "ok") {
        this->_state = WAIT_FOR_PSYNC_ANSWER;
        this->next_say<PsyncCommand>("?", "-1");
      }
    }
  } else if (this->_state == WAIT_FOR_PSYNC_ANSWER) {
    if (message.type() == Message::Type::SimpleString) {
      auto str = get<std::string>(message.getValue());

      this->_state = WAIT_FOR_RDB_FILE_SYNC;
    }
  } else if (this->_state == WAIT_FOR_RDB_FILE_SYNC) {
    if (message.type() == Message::Type::SyncResponse) {
      // auto str = get<std::string>(message.getValue());

      this->_state = WAIT_SERVER_COMMANDS;
    }
  } else if (this->_state == WAIT_SERVER_COMMANDS) {
    this->process(message);
  } else if (this->_state == UNDEFINED) {
  } else {
    this->next_say(Message::Type::Leave);
  }
}

Message::Type ReplicaTalker::expected() {
  if (this->_state == WAIT_FOR_RDB_FILE_SYNC) {
    return Message::Type::SyncResponse;
  }
  return Message::Type::Any;
}

void ReplicaTalker::set_server(ServerPtr server) {
  this->_server = std::move(server);
}

void ReplicaTalker::set_storage(IStoragePtr storage) {
  this->_storage = std::move(storage);
}

void ReplicaTalker::process(const Message& message) {
  try {
    auto command = Command::try_parse(message);
    auto type = command->type();

    if (type == CommandType::Set) {
      auto& set_command = dynamic_cast<SetCommand&>(*command);
      this->_storage->set(set_command.key(), set_command.value(), set_command.expire_ms());

    } else if (type == CommandType::ReplConf) {
      auto& replconf_command = dynamic_cast<ReplConfCommand&>(*command);

      const auto& argv = replconf_command.args();
      const auto argc = argv.size();
      if (argc != 2) {
        std::cerr << "REPLCONF expects exactly 2 arguments" << std::endl;
      }
      if (to_lower_case(argv[0]) == "getack") {
        if (argv[1] == "*") {
          this->next_say<ReplConfCommand>("ACK", std::to_string(this->_bytes_in));
        }
      }

    } else {
      std::cerr << "Unexpected command from master" << std::endl;
    }

    this->_bytes_in += message.size();
  } catch (const CommandParseError& err) {
    std::cerr << "Error in parsing command from master: " << err.what() << std::endl;
  }
}
