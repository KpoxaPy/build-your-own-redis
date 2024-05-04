#pragma once

#include "message.h"

#include <deque>
#include <vector>

template<typename T>
class MessageParser {
public:
  using RawMessage = std::string;
  using RawMessageBuffer = std::deque<RawMessage>;

  MessageParser(T& buffer);

  std::optional<Message> try_parse();

private:
  T& _buffer;
  RawMessageBuffer _raw_message_buffer;
};
