#include "replica_talker.h"

#include "command.h"

#include <algorithm>

enum : int {
  INIT = 0,
  UNDEFINED = 1,
  WAIT_FIRST_PONG = 2,
  WAIT_OK_FOR_REPLCONF_PORT = 3,
  WAIT_OK_FOR_REPLCONF_CAPA = 4,
  WAIT_FOR_PSYNC_ANSWER = 5,
  WAIT_FOR_RDB_FILE_SYNC = 6,
};

ReplicaTalker::ReplicaTalker() {
  this->_state = WAIT_FIRST_PONG;
  this->_pending.push_back(PingCommand().construct());
}

void ReplicaTalker::listen(Message message) {
  if (this->_state == WAIT_FIRST_PONG) {
    if (message.type() == Message::Type::SimpleString) {
      auto str = get<std::string>(message.getValue());
      std::transform(str.begin(), str.end(), str.begin(), [](unsigned char ch) { return std::tolower(ch); });

      if (str == "pong") {
        this->_state = WAIT_OK_FOR_REPLCONF_PORT;
        this->next_say<ReplConfCommand>("listening-port", std::to_string(this->_server->info().server.tcp_port));
      }
    }
  } else if (this->_state == WAIT_OK_FOR_REPLCONF_PORT) {
    if (message.type() == Message::Type::SimpleString) {
      auto str = get<std::string>(message.getValue());
      std::transform(str.begin(), str.end(), str.begin(), [](unsigned char ch) { return std::tolower(ch); });

      if (str == "ok") {
        this->_state = WAIT_OK_FOR_REPLCONF_CAPA;
        this->next_say<ReplConfCommand>("capa", "psync2");
      }
    }
  } else if (this->_state == WAIT_OK_FOR_REPLCONF_CAPA) {
    if (message.type() == Message::Type::SimpleString) {
      auto str = get<std::string>(message.getValue());
      std::transform(str.begin(), str.end(), str.begin(), [](unsigned char ch) { return std::tolower(ch); });

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

      this->_state = UNDEFINED;
    }
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
  this->_server = server;
}
