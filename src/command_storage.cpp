#include "command_storage.h"

#include "message.h"
#include "utils.h"

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



CommandPtr TypeCommand::try_parse(const Message& message) {
  const auto& data = std::get<std::vector<Message>>(message.getValue());
  if (data.size() < 2) {
    std::ostringstream ss;
    ss << "TYPE command must have at least 1 argument, recieved " << data.size();
    throw CommandParseError(ss.str());
  }

  if (data[1].type() != Message::Type::BulkString) {
    std::ostringstream ss;
    ss << "TYPE command must have first argument with type BulkString";
    throw CommandParseError(ss.str());
  }

  return std::make_shared<TypeCommand>(std::get<std::string>(data[1].getValue()));
}

TypeCommand::TypeCommand(std::string key)
  : _key(key) {
  this->_type = CommandType::Type;
}

const std::string& TypeCommand::key() const {
  return this->_key;
}

Message TypeCommand::construct() const {
  std::vector<Message> parts;
  parts.emplace_back(Message::Type::BulkString, "TYPE");
  parts.emplace_back(Message::Type::BulkString, this->_key);
  return Message(Message::Type::Array, parts);
}



CommandPtr XAddCommand::try_parse(const Message& message) {
  const auto& data = std::get<std::vector<Message>>(message.getValue());

  std::string key;
  InputStreamId stream_id;
  StreamPartValue values;

  if (data.size() < 5) {
    throw CommandParseError("XADD expects at least 4 arguments");
  }

  std::size_t data_pos = 1;
  while (data_pos < data.size()) {
    if (data_pos == 1) {
      if (data[data_pos].type() != Message::Type::BulkString) {
        throw CommandParseError("stream_key has invalid type");
      }
      key = std::get<std::string>(data[data_pos].getValue());
      ++data_pos;

    } else if (data_pos == 2) {
      if (data[data_pos].type() != Message::Type::BulkString) {
        throw CommandParseError("stream_id has invalid type");
      }

      try {
        stream_id = InputStreamId{std::get<std::string>(data[data_pos].getValue())};
      } catch (const StreamIdParseError& err) {
        throw CommandParseError(err.what());
      }

      ++data_pos;

    } else if (data_pos + 1 < data.size()) {
      std::string pair_key;
      std::string pair_value;

      if (data[data_pos].type() != Message::Type::BulkString) {
        throw CommandParseError("pair key has invalid type");
      }
      pair_key = std::get<std::string>(data[data_pos].getValue());

      if (data[data_pos + 1].type() != Message::Type::BulkString) {
        throw CommandParseError("pair value has invalid type");
      }
      pair_value = std::get<std::string>(data[data_pos + 1].getValue());

      values.emplace_back(std::move(pair_key), std::move(pair_value));
      data_pos += 2;
    } else {
      throw CommandParseError("expected pairs of values");
    }
  }

  return std::make_shared<XAddCommand>(std::move(key), std::move(stream_id), std::move(values));
}

XAddCommand::XAddCommand(std::string key, InputStreamId stream_id, StreamPartValue values)
  : _key(std::move(key)), _stream_id(std::move(stream_id)), _values(std::move(values)) {
  this->_type = CommandType::XAdd;
}

const std::string& XAddCommand::key() const {
  return this->_key;
}

InputStreamId XAddCommand::stream_id() const {
  return this->_stream_id;
}

const StreamPartValue& XAddCommand::values() const {
  return this->_values;
}

StreamPartValue& XAddCommand::values() {
  return this->_values;
}

Message XAddCommand::construct() const {
  std::vector<Message> parts;
  parts.emplace_back(Message::Type::BulkString, "XADD");
  parts.emplace_back(Message::Type::BulkString, this->_key);
  parts.emplace_back(Message::Type::BulkString, this->_stream_id.to_string());
  for (const auto& [key, value] : this->_values) {
    parts.emplace_back(Message::Type::BulkString, key);
    parts.emplace_back(Message::Type::BulkString, value);
  }
  return Message(Message::Type::Array, parts);
}



CommandPtr XRangeCommand::try_parse(const Message& message) {
  const auto& data = std::get<std::vector<Message>>(message.getValue());

  std::string key;
  BoundStreamId left_id;
  BoundStreamId right_id;

  if (data.size() != 4) {
    throw CommandParseError("XRANGE expects 3 arguments");
  }

  if (data[1].type() != Message::Type::BulkString) {
    throw CommandParseError("stream_key has invalid type");
  }
  key = std::get<std::string>(data[1].getValue());

  if (data[2].type() != Message::Type::BulkString) {
    throw CommandParseError("left_stream_id has invalid type");
  }

  try {
    left_id = BoundStreamId{std::get<std::string>(data[2].getValue())};
  } catch (const StreamIdParseError& err) {
    throw CommandParseError(err.what());
  }

  if (data[3].type() != Message::Type::BulkString) {
    throw CommandParseError("right_stream_id has invalid type");
  }

  try {
    right_id = BoundStreamId{std::get<std::string>(data[3].getValue())};
  } catch (const StreamIdParseError& err) {
    throw CommandParseError(err.what());
  }

  return std::make_shared<XRangeCommand>(std::move(key), std::move(left_id), std::move(right_id));
}

XRangeCommand::XRangeCommand(std::string key, BoundStreamId left_id, BoundStreamId right_id)
  : _key(key), _left_id(std::move(left_id)), _right_id(std::move(right_id)) {
  this->_type = CommandType::XRange;
}

const std::string& XRangeCommand::key() const {
  return this->_key;
}

BoundStreamId XRangeCommand::left_id() const {
  return this->_left_id;
}

BoundStreamId XRangeCommand::right_id() const {
  return this->_right_id;
}

Message XRangeCommand::construct() const {
  std::vector<Message> parts;
  parts.emplace_back(Message::Type::BulkString, "XRANGE");
  parts.emplace_back(Message::Type::BulkString, this->_key);
  parts.emplace_back(Message::Type::BulkString, this->_left_id.to_string());
  parts.emplace_back(Message::Type::BulkString, this->_right_id.to_string());
  return Message(Message::Type::Array, parts);
}



CommandPtr XReadCommand::try_parse(const Message& message) {
  const auto& data = std::get<std::vector<Message>>(message.getValue());


  if (data.size() < 4) {
    throw CommandParseError("XREAD expects at least 3 arguments");
  }

  bool met_streams = false;
  std::size_t expected_streams = 0;
  std::vector<std::string> stream_keys;
  std::vector<ReadStreamId> stream_ids;

  std::optional<std::size_t> block_ms;

  std::size_t data_pos = 1;
  while (data_pos < data.size()) {
    if (met_streams) {
      if (stream_keys.size() < expected_streams) {
        if (data[data_pos].type() != Message::Type::BulkString) {
          throw CommandParseError("stream_key has invalid type");
        }
        stream_keys.emplace_back(std::get<std::string>(data[data_pos].getValue()));
        ++data_pos;

      } else if (stream_ids.size() < expected_streams) {
        if (data[data_pos].type() != Message::Type::BulkString) {
          throw CommandParseError("stream_id has invalid type");
        }

        try {
          stream_ids.emplace_back(ReadStreamId{std::get<std::string>(data[data_pos].getValue())});
        } catch (const StreamIdParseError& err) {
          throw CommandParseError(err.what());
        }

        ++data_pos;
      } else {
        throw CommandParseError("unxepected arg after streams");
      }
    } else {
      if (data[data_pos].type() != Message::Type::BulkString) {
        throw CommandParseError("expected bulk string for arg");
      }

      auto arg = to_lower_case(std::get<std::string>(data[data_pos].getValue()));

      if (arg == "streams") {
        met_streams = true;
        auto remaining_args_count = data.size() - data_pos - 1;

        if (remaining_args_count < 2) {
          throw CommandParseError("XREAD expects at least one full pair of key and id for streams");
        }

        expected_streams = remaining_args_count / 2;
        stream_keys.reserve(expected_streams);
        stream_ids.reserve(expected_streams);

        ++data_pos;
      } else if (arg == "block") {
        if (data_pos + 1 >= data.size()) {
          throw CommandParseError("XREAD expects an argument after block");
        }

        if (data[data_pos + 1].type() != Message::Type::BulkString) {
          throw CommandParseError("XREAD block must have argument with type BulkString");
        }

        auto timeout_ms = parseUInt64(std::get<std::string>(data[data_pos + 1].getValue()));
        if (!timeout_ms) {
          throw CommandParseError("XREAD block must have argument as number");
        }
        block_ms = timeout_ms.value();

        data_pos += 2;
      } else {
        throw CommandParseError(print_args("unexpected arg for XREAD: ", arg));
      }
    }
  }

  StreamsReadRequest request;
  request.reserve(expected_streams);

  for (std::size_t i = 0; i < expected_streams; ++i) {
    request.emplace_back(std::move(stream_keys[i]), std::move(stream_ids[i]));
  }

  return std::make_shared<XReadCommand>(std::move(request), block_ms);
}

XReadCommand::XReadCommand(StreamsReadRequest request, std::optional<std::size_t> block_ms)
    : _request(std::move(request)), _block_ms(block_ms) {
  this->_type = CommandType::XRead;
}

StreamsReadRequest& XReadCommand::request() {
  return this->_request;
}

const std::optional<std::size_t>& XReadCommand::block_ms() {
  return this->_block_ms;
}

Message XReadCommand::construct() const {
  std::vector<Message> parts;
  parts.emplace_back(Message::Type::BulkString, "XREAD");

  if (this->_block_ms) {
    parts.emplace_back(Message::Type::BulkString, "block");
    parts.emplace_back(Message::Type::BulkString, std::to_string(this->_block_ms.value()));
  }

  parts.emplace_back(Message::Type::BulkString, "streams");
  for (const auto& [key, id] : this->_request) {
    parts.emplace_back(Message::Type::BulkString, key);
    parts.emplace_back(Message::Type::BulkString, id.to_string());
  }
  return Message(Message::Type::Array, parts);
}
