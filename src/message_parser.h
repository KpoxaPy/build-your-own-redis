#pragma once

#include "message.h"

#include <deque>
#include <optional>
#include <vector>

template<typename T>
class MessageParser {
public:
  using RawMessage = std::string;
  using RawMessageBuffer = std::deque<RawMessage>;

  MessageParser(T& buffer);

  std::optional<Message> try_parse(Message::Type expected);

private:
  T& _buffer;
  RawMessageBuffer _raw_message_buffer;
  std::optional<std::size_t> _length_encoded_message_expected;
};
