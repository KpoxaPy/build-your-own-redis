#pragma once

#include "command.h"
#include "storage.h"

class SetCommand : public Command {
public:
  static CommandPtr try_parse(const Message&);

  SetCommand(std::string key, std::string value, std::optional<int> expire_ms = {});
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

class TypeCommand : public Command {
public:
  static CommandPtr try_parse(const Message&);

  TypeCommand(std::string key);

  const std::string& key() const;

  Message construct() const override;

private:
  std::string _key;
};

class XAddCommand : public Command {
public:
  static CommandPtr try_parse(const Message&);

  XAddCommand(std::string key, InputStreamId stream_id, StreamPartValue values);

  const std::string& key() const;

  InputStreamId stream_id() const;

  const StreamPartValue& values() const;
  StreamPartValue& values();

  Message construct() const override;

private:
  std::string _key;
  InputStreamId _stream_id;
  StreamPartValue _values;
};

class XRangeCommand : public Command {
public:
  static CommandPtr try_parse(const Message&);

  XRangeCommand(std::string key, BoundStreamId left_id, BoundStreamId right_id);

  const std::string& key() const;

  BoundStreamId left_id() const;
  BoundStreamId right_id() const;

  Message construct() const override;

private:
  std::string _key;
  BoundStreamId _left_id;
  BoundStreamId _right_id;
};

class XReadCommand : public Command {
public:
  static CommandPtr try_parse(const Message&);

  XReadCommand(StreamsReadRequest, std::optional<std::size_t> block_ms);

  StreamsReadRequest& request();
  const std::optional<std::size_t>& block_ms();

  Message construct() const override;

private:
  StreamsReadRequest _request;
  std::optional<std::size_t> _block_ms;
};
