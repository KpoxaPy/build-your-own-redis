#include "replica_talker.h"

#include "command.h"

std::optional<Message> ReplicaTalker::talk() {
  return PingCommand().construct();
}

std::optional<Message> ReplicaTalker::talk(const Message& message) {
  return Message(Message::Type::Undefined);
}