#include "server_talker.h"

std::optional<Message> ServerTalker::talk() {
  return {};
}

std::optional<Message> ServerTalker::talk(const Message &) {
  return {};
}

// void Handler::reply(CommandPtr command) {
//   if (command->type() == CommandType::Ping) {
//     this->send(Message(Message::Type::SimpleString, {"PONG"}));

//   } else if (command->type() == CommandType::Echo) {
//     auto echo_command = static_cast<EchoCommand&>(*command);

//     this->send(Message(Message::Type::BulkString, echo_command.data()));

//   } else if (command->type() == CommandType::Set) {
//     auto set_command = static_cast<SetCommand&>(*command);

//     this->_manager.storage()->storage[set_command.key()] = set_command.value();
//     Value& stored_value = this->_manager.storage()->storage[set_command.key()];

//     if (set_command.expire_ms()) {
//       stored_value.setExpire(std::chrono::milliseconds{set_command.expire_ms().value()});
//     }

//     this->send(Message(Message::Type::SimpleString, "OK"));

//   } else if (command->type() == CommandType::Get) {
//     auto get_command = static_cast<GetCommand&>(*command);

//     auto it = this->_manager.storage()->storage.find(get_command.key());
//     if (it == this->_manager.storage()->storage.end()) {
//       this->send(Message(Message::Type::BulkString, {}));
//       return;
//     }
//     auto& value = it->second;

//     if (value.getExpire() && Clock::now() >= value.getExpire()) {
//       this->_manager.storage()->storage.erase(get_command.key());
//       this->send(Message(Message::Type::BulkString, {}));
//       return;
//     }

//     this->send(Message(Message::Type::BulkString, value.data()));

//   } else if (command->type() == CommandType::Info) {
//     auto info_command = static_cast<InfoCommand&>(*command);

//     std::unordered_set<std::string> info_parts;

//     auto default_parts = [&info_parts]() {
//       info_parts.insert("server");
//       info_parts.insert("replication");
//     };

//     for (const auto& info_part : info_command.args()) {
//       if (info_part == "default") {
//         default_parts();
//       } else {
//         info_parts.insert(info_part);
//       }
//     }

//     if (info_parts.size() == 0) {
//       default_parts();
//     }

//     this->send(Message(Message::Type::BulkString, this->_manager.server()->info().to_string(info_parts)));

//   } else {
//     this->send(Message(Message::Type::SimpleError, {"unimplemented command"}));
//   }
// }