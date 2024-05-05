#include "replica_talker.h"

#include "command.h"

#include <algorithm>

std::optional<Message> ReplicaTalker::talk() {
  this->_state = "wait pong";
  return PingCommand().construct();
}

std::optional<Message> ReplicaTalker::talk(const Message& message) {
  if (this->_state == "wait pong") {
    if (message.type() == Message::Type::SimpleString) {
      auto str = get<std::string>(message.getValue());
      std::transform(str.begin(), str.end(), str.begin(), [](unsigned char ch) { return std::tolower(ch); });

      if (str == "pong") {
        this->_state = "wait ok on replconf port";
        return ReplConfCommand({"listening-port", std::to_string(this->_server->info().server.tcp_port)}).construct();
      }
    }
  } else if (this->_state == "wait ok on replconf port") {
    if (message.type() == Message::Type::SimpleString) {
      auto str = get<std::string>(message.getValue());
      std::transform(str.begin(), str.end(), str.begin(), [](unsigned char ch) { return std::tolower(ch); });

      if (str == "ok") {
        this->_state = "wait ok on replconf capa";
        return ReplConfCommand({"capa", "psync2"}).construct();
      }
    }
  } else if (this->_state == "wait ok on replconf capa") {
    if (message.type() == Message::Type::SimpleString) {
      auto str = get<std::string>(message.getValue());
      std::transform(str.begin(), str.end(), str.begin(), [](unsigned char ch) { return std::tolower(ch); });

      if (str == "ok") {
        this->_state = "wait answer on psync";
        return PsyncCommand({"?", "-1"}).construct();
      }
    }
  } else if (this->_state == "wait answer on psync") {
    if (message.type() == Message::Type::SimpleString) {
      auto str = get<std::string>(message.getValue());

      this->_state = "undef";
      // return PsyncCommand({"?", "-1"}).construct();
      return Message(Message::Type::Undefined);
    }
  } else {
    return Message(Message::Type::Undefined);
  }

  return {};
}

void ReplicaTalker::set_server(ServerPtr server) {
  this->_server = server;
}
