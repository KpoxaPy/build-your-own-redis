#pragma once

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
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
  Info,
  ReplConf,
  Psync,
  Wait,
  Keys,
  Config,
  Type,
  XAdd,
  XRange,
  XRead,
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

class InfoCommand : public Command {
public:
  static CommandPtr try_parse(const Message&);

  template <typename... Args>
  InfoCommand(Args&&... args) : InfoCommand({std::forward<Args>(args)...}) {}
  InfoCommand(std::initializer_list<std::string> args = {});

  const std::vector<std::string>& args() const;

  Message construct() const override;

private:
  std::vector<std::string> _args;
};

class ReplConfCommand : public Command {
public:
  static CommandPtr try_parse(const Message&);

  template <typename... Args>
  ReplConfCommand(Args&&... args) : ReplConfCommand({std::forward<Args>(args)...}) {}
  ReplConfCommand(std::initializer_list<std::string> args = {});

  const std::vector<std::string>& args() const;

  Message construct() const override;

private:
  std::vector<std::string> _args;
};

class PsyncCommand : public Command {
public:
  static CommandPtr try_parse(const Message&);

  template <typename... Args>
  PsyncCommand(Args&&... args) : PsyncCommand({std::forward<Args>(args)...}) {}
  PsyncCommand(std::initializer_list<std::string> args = {});

  const std::vector<std::string>& args() const;

  Message construct() const override;

private:
  std::vector<std::string> _args;
};

class WaitCommand : public Command {
public:
  static CommandPtr try_parse(const Message&);

  WaitCommand(std::size_t replicas, std::size_t timeout_ms);
  std::size_t replicas() const;
  std::size_t timeout_ms() const;

  Message construct() const override;

private:
  std::size_t _replicas;
  std::size_t _timeout_ms;
};

class KeysCommand : public Command {
public:
  static CommandPtr try_parse(const Message&);

  KeysCommand(std::string arg);
  const std::string& arg() const;

  Message construct() const override;

private:
  std::string _arg;
};

class ConfigCommand : public Command {
public:
  static CommandPtr try_parse(const Message&);

  template <typename... Args>
  ConfigCommand(std::string action, Args&&... args) : ConfigCommand(std::move(action), {std::forward<Args>(args)...}) {}
  ConfigCommand(std::string action, std::initializer_list<std::string> args);

  const std::string& action() const;
  const std::vector<std::string>& args() const;

  Message construct() const override;

private:
  std::string _action;
  std::vector<std::string> _args;
};
