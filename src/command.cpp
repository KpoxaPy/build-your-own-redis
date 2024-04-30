#include "command.h"

#include "client.h"
#include "message.h"
#include "server.h"
#include "utils.h"

#include <unordered_set>

CommandParseError::CommandParseError(std::string reason)
  : std::runtime_error(reason)
{
}

CommandPtr Command::try_parse(const Message& message) {
  if (message.type() != Message::Type::Array) {
    throw CommandParseError("unknown command");
  }

  const auto& data = std::get<std::vector<Message>>(message.getValue());
  if (data.size() == 0) {
    throw CommandParseError("unknown command");
  }

  if (data[0].type() != Message::Type::BulkString) {
    throw CommandParseError("unknown command");
  }

  std::string command = std::get<std::string>(data[0].getValue());
  std::transform(command.begin(), command.end(), command.begin(), [](unsigned char ch) { return std::tolower(ch); });

  if (command == "ping") {
    return PingCommand::try_parse(message);
  } else if (command == "echo") {
    return EchoCommand::try_parse(message);
  } else if (command == "set") {
    return SetCommand::try_parse(message);
  } else if (command == "get") {
    return GetCommand::try_parse(message);
  } else if (command == "info") {
    return InfoCommand::try_parse(message);
  }

  throw CommandParseError("unknown command");
}

void Command::send(Client& client) const {
  client.send(this->construct());
}

void Command::reply(Client&) const {
  throw std::runtime_error("Command::reply should not be invoked");
}

Message Command::construct() const {
  throw std::runtime_error("Command::construct should not be invoked");
}

CommandPtr PingCommand::try_parse(const Message&) {
  return std::make_shared<PingCommand>();
}

void PingCommand::reply(Client& client) const {
  client.send(Message(Message::Type::SimpleString, {"PONG"}));
}

Message PingCommand::construct() const {
  std::vector<Message> parts;
  parts.emplace_back(Message::Type::BulkString, "PING");
  return Message(Message::Type::Array, parts);
}



CommandPtr EchoCommand::try_parse(const Message& message) {
  const auto& data = std::get<std::vector<Message>>(message.getValue());
  if (data.size() != 2) {
    std::ostringstream ss;
    ss << "ECHO command must have 1 argument, recieved " << data.size();
    throw CommandParseError(ss.str());
  }

  if (data[1].type() != Message::Type::BulkString) {
    std::ostringstream ss;
    ss << "ECHO command must have first argument with type BulkString";
    throw CommandParseError(ss.str());
  }

  return std::make_shared<EchoCommand>(std::get<std::string>(data[1].getValue()));
}

EchoCommand::EchoCommand(std::string data)
  : _data(data)
{
}

void EchoCommand::reply(Client& client) const {
  client.send(Message(Message::Type::BulkString, this->_data));
}

Message EchoCommand::construct() const {
  std::vector<Message> parts;
  parts.emplace_back(Message::Type::BulkString, "ECHO");
  parts.emplace_back(Message::Type::BulkString, this->_data);
  return Message(Message::Type::Array, parts);
}



CommandPtr SetCommand::try_parse(const Message& message) {
  const auto& data = std::get<std::vector<Message>>(message.getValue());

  std::optional<std::string> key;
  std::optional<std::string> value;
  std::optional<int> expire_ms;

  std::size_t data_pos = 1;
  while (data_pos < data.size()) {
    if (data_pos == 1) {
      if (data[data_pos].type() != Message::Type::BulkString) {
        throw CommandParseError("invalid type");
      }
      key = std::get<std::string>(data[data_pos].getValue());
      ++data_pos;

    } else if (data_pos == 2) {
      if (data[data_pos].type() != Message::Type::BulkString) {
        throw CommandParseError("invalid type");
      }
      value = std::get<std::string>(data[data_pos].getValue());
      ++data_pos;

    } else {
      if (data[data_pos].type() != Message::Type::BulkString) {
        throw CommandParseError("invalid type");
      }
      std::string param = std::get<std::string>(data[data_pos].getValue());
      std::transform(param.begin(), param.end(), param.begin(), [](unsigned char ch) { return std::tolower(ch); });

      if (param == "px") {
        if (data_pos + 1 >= data.size()) {
          std::ostringstream ss;
          ss << "param " << std::quoted(param) << " requires argument";
          throw CommandParseError(ss.str());
        }

        if (data[data_pos + 1].type() != Message::Type::BulkString) {
          throw CommandParseError("invalid px argument type");
        }

        const std::string& px_value_str = std::get<std::string>(data[data_pos + 1].getValue());
        std::optional<int> px_value = parseInt(px_value_str.data(), px_value_str.size());

        if (!px_value || px_value.value() <= 0) {
          throw CommandParseError("invalid px argument value");
        }

        expire_ms = px_value.value();
        data_pos += 2;

      } else {
        std::ostringstream ss;
        ss << "unknown param " << std::quoted(param);
        throw CommandParseError(ss.str());
      }
    }
  }

  if (!key || !value) {
    throw CommandParseError("not enough arguments");
  }

  auto command = std::make_shared<SetCommand>(key.value(), value.value());

  if (expire_ms) {
    command->setExpireMs(expire_ms.value());
  }

  return command;
}

SetCommand::SetCommand(std::string key, std::string value)
  : _key(key)
  , _value(value)
{
}

void SetCommand::setExpireMs(int ms) {
  this->_expire_ms = ms;
}

void SetCommand::reply(Client& client) const {
  client.storage().storage[this->_key] = this->_value;
  Value& stored_value = client.storage().storage[this->_key];

  if (this->_expire_ms) {
    stored_value.setExpire(std::chrono::milliseconds{this->_expire_ms.value()});
  }

  client.send(Message(Message::Type::SimpleString, "OK"));
}

Message SetCommand::construct() const {
  std::vector<Message> parts;
  parts.emplace_back(Message::Type::BulkString, "SET");
  parts.emplace_back(Message::Type::BulkString, this->_key);
  parts.emplace_back(Message::Type::BulkString, this->_value);

  if (this->_expire_ms) {
    parts.emplace_back(Message::Type::BulkString, "PX");
    parts.emplace_back(Message::Type::BulkString, std::to_string(this->_expire_ms.value()));
  }

  return Message(Message::Type::Array, parts);
}



CommandPtr GetCommand::try_parse(const Message& message) {
  const auto& data = std::get<std::vector<Message>>(message.getValue());
  if (data.size() < 2) {
    std::ostringstream ss;
    ss << "GET command must have at least 1 argument, recieved " << data.size();
    throw CommandParseError(ss.str());
  }

  if (data[1].type() != Message::Type::BulkString) {
    std::ostringstream ss;
    ss << "GET command must have first argument with type BulkString";
    throw CommandParseError(ss.str());
  }

  return std::make_shared<GetCommand>(std::get<std::string>(data[1].getValue()));
}

GetCommand::GetCommand(std::string key)
  : _key(key)
{
}

void GetCommand::reply(Client& client) const {
  auto it = client.storage().storage.find(this->_key);
  if (it == client.storage().storage.end()) {
    client.send(Message(Message::Type::BulkString, {}));
    return;
  }
  auto& value = it->second;

  if (value.getExpire() && Clock::now() >= value.getExpire()) {
    client.storage().storage.erase(this->_key);
    client.send(Message(Message::Type::BulkString, {}));
    return;
  }

  client.send(Message(Message::Type::BulkString, value.data()));
}

Message GetCommand::construct() const {
  std::vector<Message> parts;
  parts.emplace_back(Message::Type::BulkString, "GET");
  parts.emplace_back(Message::Type::BulkString, this->_key);
  return Message(Message::Type::Array, parts);
}



void insert_info_parts_default(std::unordered_set<std::string>& info_parts) {
  info_parts.insert("server");
  info_parts.insert("replication");
}

CommandPtr InfoCommand::try_parse(const Message& message) {
  const auto& data = std::get<std::vector<Message>>(message.getValue());
  auto command = std::make_shared<InfoCommand>();

  std::size_t data_pos = 1;
  while (data_pos < data.size()) {
    if (data[data_pos].type() != Message::Type::BulkString) {
      throw CommandParseError("invalid type");
    }

    std::string info_part = std::get<std::string>(data[data_pos].getValue());
    std::transform(info_part.begin(), info_part.end(), info_part.begin(), [](unsigned char ch) { return std::tolower(ch); });
    command->setInfoPart(info_part);

    data_pos += 1;
  }

  return command;
}

void InfoCommand::setInfoPart(std::string info_part) {
  this->_args.emplace_back(std::move(info_part));
}

void InfoCommand::reply(Client& client) const {
  std::unordered_set<std::string> info_parts;

  for (const auto& info_part: this->_args) {
    if (info_part == "default") {
      insert_info_parts_default(info_parts);
    } else {
      info_parts.insert(info_part);
    }
  }

  if (info_parts.size() == 0) {
    insert_info_parts_default(info_parts);
  }

  client.send(Message(Message::Type::BulkString, client.server().info().to_string(info_parts)));
}

Message InfoCommand::construct() const {
  std::vector<Message> parts;
  parts.emplace_back(Message::Type::BulkString, "INFO");
  for (const auto& info_part: this->_args) {
    parts.emplace_back(Message::Type::BulkString, info_part);
  }
  return Message(Message::Type::Array, parts);
}
