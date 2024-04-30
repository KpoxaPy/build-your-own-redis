#pragma once

#include "message.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

class Client;

class CommandParseError : public std::runtime_error {
public:
  CommandParseError(std::string);
};

class Command;
using CommandPtr = std::shared_ptr<Command>;

class Command {
public:
  static CommandPtr try_parse(const Message&);

  virtual void send(Client&) const;
  virtual void reply(Client&) const;

  virtual Message construct() const;
};

class PingCommand : public Command {
public:
  static CommandPtr try_parse(const Message&);

  void reply(Client&) const override;

  Message construct() const override;
};

class EchoCommand : public Command {
public:
  static CommandPtr try_parse(const Message&);

  EchoCommand(std::string);

  void reply(Client&) const override;

  Message construct() const override;

private:
  std::string _data;
};

class SetCommand : public Command {
public:
  static CommandPtr try_parse(const Message&);

  SetCommand(std::string key, std::string value);
  void setExpireMs(int);

  void reply(Client&) const override;

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

  void reply(Client&) const override;

  Message construct() const override;

private:
  std::string _key;
};

class InfoCommand : public Command {
public:
  static CommandPtr try_parse(const Message&);

  InfoCommand() = default;

  void setInfoPart(std::string);

  void reply(Client&) const override;

  Message construct() const override;

private:
  std::vector<std::string> _args;
};
