#pragma once

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

class Message;

class CommandParseError : public std::runtime_error {
public:
  CommandParseError(std::string);
};

enum class CommandType {
  Unknown,
  Ping,
  Echo,
  Set,
  Get,
  Info
};

class Command;
using CommandPtr = std::shared_ptr<Command>;

class Command {
public:
  static CommandPtr try_parse(const Message&);

  virtual ~Command() = default;

  virtual Message construct() const;

  CommandType type() const;

protected:
  CommandType _type;
};

class PingCommand : public Command {
public:
  static CommandPtr try_parse(const Message&);

  PingCommand();

  Message construct() const override;
};

class EchoCommand : public Command {
public:
  static CommandPtr try_parse(const Message&);

  EchoCommand(std::string);

  const std::string& data() const;

  Message construct() const override;

private:
  std::string _data;
};

class SetCommand : public Command {
public:
  static CommandPtr try_parse(const Message&);

  SetCommand(std::string key, std::string value, std::optional<int> expire_ms);
  const std::string& key() const;
  const std::string& value() const;
  const std::optional<int>& expire_ms() const;

  Message construct() const override;

private:
  std::string _key;
  std::string _value;
  std::optional<int> _expire_ms;
};

class GetCommand : public Command {
public:
  static CommandPtr try_parse(const Message&);

  GetCommand(std::string key);

  const std::string& key() const;

  Message construct() const override;

private:
  std::string _key;
};

class InfoCommand : public Command {
public:
  static CommandPtr try_parse(const Message&);

  InfoCommand(std::vector<std::string> args = {});

  const std::vector<std::string>& args() const;

  Message construct() const override;

private:
  std::vector<std::string> _args;
};
