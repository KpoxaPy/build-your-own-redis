#include "command.h"

#include "command_storage.h"
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

  const std::string command = to_lower_case(std::get<std::string>(data[0].getValue()));
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
  } else if (command == "wait") {
    return WaitCommand::try_parse(message);
  } else if (command == "keys") {
    return KeysCommand::try_parse(message);
  } else if (command == "config") {
    return ConfigCommand::try_parse(message);
  } else if (command == "type") {
    return TypeCommand::try_parse(message);
  } else if (command == "xadd") {
    return XAddCommand::try_parse(message);
  } else if (command == "xrange") {
    return XRangeCommand::try_parse(message);
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



CommandPtr WaitCommand::try_parse(const Message& message) {
  const auto& data = std::get<std::vector<Message>>(message.getValue());
  if (data.size() == 4) {
    std::ostringstream ss;
    ss << "WAIT command must have 2 arguments, recieved " << data.size() - 1;
    throw CommandParseError(ss.str());
  }

  if (data[1].type() != Message::Type::BulkString) {
    std::ostringstream ss;
    ss << "WAIT command must have first argument with type BulkString";
    throw CommandParseError(ss.str());
  }

  const std::string& replicas_str = std::get<std::string>(data[1].getValue());
  std::optional<int> replicas = parseInt(replicas_str.data(), replicas_str.size());

  if (!replicas) {
    std::ostringstream ss;
    ss << "WAIT command must have first argument as number";
    throw CommandParseError(ss.str());
  }

  if (data[2].type() != Message::Type::BulkString) {
    std::ostringstream ss;
    ss << "WAIT command must have second argument with type BulkString";
    throw CommandParseError(ss.str());
  }

  const std::string& timeout_ms_str = std::get<std::string>(data[2].getValue());
  std::optional<int> timeout_ms = parseInt(timeout_ms_str.data(), timeout_ms_str.size());

  if (!timeout_ms) {
    std::ostringstream ss;
    ss << "WAIT command must have second argument as number";
    throw CommandParseError(ss.str());
  }

  return std::make_shared<WaitCommand>(replicas.value(), timeout_ms.value());
}

WaitCommand::WaitCommand(std::size_t replicas, std::size_t timeout_ms)
  : _replicas(replicas), _timeout_ms(timeout_ms) {
  this->_type = CommandType::Wait;
}

std::size_t WaitCommand::replicas() const {
  return this->_replicas;
}

std::size_t WaitCommand::timeout_ms() const {
  return this->_timeout_ms;
}

Message WaitCommand::construct() const {
  std::vector<Message> parts;
  parts.emplace_back(Message::Type::BulkString, "WAIT");
  parts.emplace_back(Message::Type::BulkString, std::to_string(this->_replicas));
  parts.emplace_back(Message::Type::BulkString, std::to_string(this->_timeout_ms));

  return Message(Message::Type::Array, parts);
}



CommandPtr KeysCommand::try_parse(const Message& message) {
  const auto& data = std::get<std::vector<Message>>(message.getValue());
  if (data.size() < 2) {
    std::ostringstream ss;
    ss << "KEYS command must have at least 1 argument, recieved " << data.size();
    throw CommandParseError(ss.str());
  }

  if (data[1].type() != Message::Type::BulkString) {
    std::ostringstream ss;
    ss << "KEYS command must have first argument with type BulkString";
    throw CommandParseError(ss.str());
  }

  return std::make_shared<KeysCommand>(std::get<std::string>(data[1].getValue()));
}

KeysCommand::KeysCommand(std::string arg)
  : _arg(arg) {
  this->_type = CommandType::Keys;
}

const std::string& KeysCommand::arg() const {
  return this->_arg;
}

Message KeysCommand::construct() const {
  std::vector<Message> parts;
  parts.emplace_back(Message::Type::BulkString, "KEYS");
  parts.emplace_back(Message::Type::BulkString, this->_arg);

  return Message(Message::Type::Array, parts);
}



CommandPtr ConfigCommand::try_parse(const Message& message) {
  const auto& data = std::get<std::vector<Message>>(message.getValue());
  auto command = std::make_shared<ConfigCommand>("");

  if (data.size() <= 1) {
    return command;
  }

  if (data[1].type() != Message::Type::BulkString) {
    std::ostringstream ss;
    ss << "CONFIG command must have first argument with type BulkString";
    throw CommandParseError(ss.str());
  }
  command->_action = std::get<std::string>(data[1].getValue());

  std::size_t data_pos = 2;
  while (data_pos < data.size()) {
    if (data[data_pos].type() != Message::Type::BulkString) {
      throw CommandParseError("CONFIG arguments should be BulkString");
    }

    command->_args.emplace_back(std::get<std::string>(data[data_pos].getValue()));
    data_pos += 1;
  }

  return command;
}

ConfigCommand::ConfigCommand(std::string action, std::initializer_list<std::string> args)
  : _action(std::move(action)), _args(args) {
  this->_type = CommandType::Config;
}

const std::string& ConfigCommand::action() const {
  return this->_action;
}

const std::vector<std::string>& ConfigCommand::args() const {
  return this->_args;
}

Message ConfigCommand::construct() const {
  std::vector<Message> parts;
  parts.emplace_back(Message::Type::BulkString, "CONFIG");
  parts.emplace_back(Message::Type::BulkString, to_upper_case(this->_action));

  for (const auto& arg: this->_args) {
    parts.emplace_back(Message::Type::BulkString, arg);
  }

  return Message(Message::Type::Array, parts);
}
