#include "replica_talker.h"

#include "command.h"

#include <algorithm>

enum : int {
  INIT = 0,
  UNDEFINED = 1,
  WAIT_FIRST_PONG = 2,
  WAIT_OK_FOR_REPLCONF_PORT = 3,
  WAIT_OK_FOR_REPLCINF_CAPA = 4,
  WAIT_FOR_PSYNC_ANSWER = 5,
  WAIT_FOR_RDB_FILE_SYNC = 6,
};

std::optional<Message> ReplicaTalker::talk() {
  this->_state = WAIT_FIRST_PONG;
  return PingCommand().construct();
}

std::optional<Message> ReplicaTalker::talk(const Message& message) {
  if (this->_state == WAIT_FIRST_PONG) {
    if (message.type() == Message::Type::SimpleString) {
      auto str = get<std::string>(message.getValue());
      std::transform(str.begin(), str.end(), str.begin(), [](unsigned char ch) { return std::tolower(ch); });

      if (str == "pong") {
        this->_state = WAIT_OK_FOR_REPLCONF_PORT;
        return ReplConfCommand({"listening-port", std::to_string(this->_server->info().server.tcp_port)}).construct();
      }
    }
  } else if (this->_state == WAIT_OK_FOR_REPLCONF_PORT) {
    if (message.type() == Message::Type::SimpleString) {
      auto str = get<std::string>(message.getValue());
      std::transform(str.begin(), str.end(), str.begin(), [](unsigned char ch) { return std::tolower(ch); });

      if (str == "ok") {
        this->_state = WAIT_OK_FOR_REPLCINF_CAPA;
        return ReplConfCommand({"capa", "psync2"}).construct();
      }
    }
  } else if (this->_state == WAIT_OK_FOR_REPLCINF_CAPA) {
    if (message.type() == Message::Type::SimpleString) {
      auto str = get<std::string>(message.getValue());
      std::transform(str.begin(), str.end(), str.begin(), [](unsigned char ch) { return std::tolower(ch); });

      if (str == "ok") {
        this->_state = WAIT_FOR_PSYNC_ANSWER;
        return PsyncCommand({"?", "-1"}).construct();
      }
    }
  } else if (this->_state == WAIT_FOR_PSYNC_ANSWER) {
    if (message.type() == Message::Type::SimpleString) {
      auto str = get<std::string>(message.getValue());

      this->_state = WAIT_FOR_RDB_FILE_SYNC;
      return Message(Message::Type::Undefined);
    }
  } else if (this->_state == WAIT_FOR_RDB_FILE_SYNC) {
    if (message.type() == Message::Type::SyncResponse) {
      // auto str = get<std::string>(message.getValue());

      this->_state = UNDEFINED;
      return Message();
    }
  } else {
    return Message();
  }

  return {};
}

Message::Type ReplicaTalker::expected() {
  if (this->_state == WAIT_FOR_RDB_FILE_SYNC) {
    return Message::Type::SyncResponse;
  }
  return Message::Type::Any;
}

void ReplicaTalker::set_server(ServerPtr server) {
  this->_server = server;
}
