#include "command.h"

#include "message.h"
#include "utils.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
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

  std::string command = to_lower_case(std::get<std::string>(data[0].getValue()));
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
  } else if (command == "replconf") {
    return ReplConfCommand::try_parse(message);
  } else if (command == "psync") {
    return PsyncCommand::try_parse(message);
  }

  throw CommandParseError("unknown command");
}

Message Command::construct() const {
  throw std::runtime_error("Command::construct should not be invoked");
}

CommandType Command::type() const {
  return this->_type;
}

CommandPtr PingCommand::try_parse(const Message&) {
  return std::make_shared<PingCommand>();
}

PingCommand::PingCommand() {
  this->_type = CommandType::Ping;
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
    : _data(std::move(data)) {
  this->_type = CommandType::Echo;
}

const std::string& EchoCommand::data() const {
  return this->_data;
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
      std::string param = to_lower_case(std::get<std::string>(data[data_pos].getValue()));

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

  return std::make_shared<SetCommand>(key.value(), value.value(), expire_ms);
}

SetCommand::SetCommand(std::string key, std::string value, std::optional<int> expire_ms)
  : _key(key)
  , _value(value)
  , _expire_ms(expire_ms)
{
  this->_type = CommandType::Set;
}

const std::string& SetCommand::key() const {
  return this->_key;
}

const std::string& SetCommand::value() const {
  return this->_value;
}

const std::optional<int>& SetCommand::expire_ms() const {
  return this->_expire_ms;
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
  this->_type = CommandType::Get;
}

const std::string& GetCommand::key() const {
  return this->_key;
}

Message GetCommand::construct() const {
  std::vector<Message> parts;
  parts.emplace_back(Message::Type::BulkString, "GET");
  parts.emplace_back(Message::Type::BulkString, this->_key);
  return Message(Message::Type::Array, parts);
}



CommandPtr InfoCommand::try_parse(const Message& message) {
  const auto& data = std::get<std::vector<Message>>(message.getValue());
  auto command = std::make_shared<InfoCommand>();

  std::size_t data_pos = 1;
  while (data_pos < data.size()) {
    if (data[data_pos].type() != Message::Type::BulkString) {
      throw CommandParseError("invalid type");
    }

    command->_args.emplace_back(to_lower_case(std::get<std::string>(data[data_pos].getValue())));

    data_pos += 1;
  }

  return command;
}

InfoCommand::InfoCommand(std::initializer_list<std::string> args)
  : _args(args) {
  this->_type = CommandType::Info;
}

const std::vector<std::string>& InfoCommand::args() const {
  return this->_args;
}

Message InfoCommand::construct() const {
  std::vector<Message> parts;
  parts.emplace_back(Message::Type::BulkString, "INFO");
  for (const auto& info_part: this->_args) {
    parts.emplace_back(Message::Type::BulkString, info_part);
  }
  return Message(Message::Type::Array, parts);
}



CommandPtr ReplConfCommand::try_parse(const Message& message) {
  const auto& data = std::get<std::vector<Message>>(message.getValue());
  auto command = std::make_shared<ReplConfCommand>();

  std::size_t data_pos = 1;
  while (data_pos < data.size()) {
    if (data[data_pos].type() != Message::Type::BulkString) {
      throw CommandParseError("invalid type");
    }

    command->_args.emplace_back(std::get<std::string>(data[data_pos].getValue()));
    data_pos += 1;
  }

  return command;
}

ReplConfCommand::ReplConfCommand(std::initializer_list<std::string> args)
  : _args(args) {
  this->_type = CommandType::ReplConf;
}

const std::vector<std::string>& ReplConfCommand::args() const {
  return this->_args;
}

Message ReplConfCommand::construct() const {
  std::vector<Message> parts;
  parts.emplace_back(Message::Type::BulkString, "REPLCONF");
  for (const auto& info_part: this->_args) {
    parts.emplace_back(Message::Type::BulkString, info_part);
  }
  return Message(Message::Type::Array, parts);
}



CommandPtr PsyncCommand::try_parse(const Message& message) {
  const auto& data = std::get<std::vector<Message>>(message.getValue());
  auto command = std::make_shared<PsyncCommand>();

  std::size_t data_pos = 1;
  while (data_pos < data.size()) {
    if (data[data_pos].type() != Message::Type::BulkString) {
      throw CommandParseError("invalid type");
    }

    command->_args.emplace_back(std::get<std::string>(data[data_pos].getValue()));
    data_pos += 1;
  }

  return command;
}

PsyncCommand::PsyncCommand(std::initializer_list<std::string> args)
  : _args(args) {
  this->_type = CommandType::Psync;
}

const std::vector<std::string>& PsyncCommand::args() const {
  return this->_args;
}

Message PsyncCommand::construct() const {
  std::vector<Message> parts;
  parts.emplace_back(Message::Type::BulkString, "PSYNC");
  for (const auto& info_part: this->_args) {
    parts.emplace_back(Message::Type::BulkString, info_part);
  }
  return Message(Message::Type::Array, parts);
}