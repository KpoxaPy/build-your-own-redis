#pragma once

#include "types.h"
#include "message.h"

#include <chrono>
#include <cstddef>
#include <iostream>
#include <optional>
#include <deque>
#include <string>
#include <vector>

class Client {
public:
  enum class ProcessStatus {
    Keep,
    Closed,
  };

  Client(int fd);
  Client(Client&&);
  ~Client();

  ProcessStatus process();

private:
  std::optional<int> _client_fd;
  std::chrono::steady_clock::time_point _begin_time;
  RawMessageBuffer _buffer;
  RawMessagesStream _raw_messages;

  void reply_to(const Message&);
  void reply_to_echo(const Message&);
  void reply_to_ping();
  void reply_unknown();

  void close();

  std::size_t read();
  std::size_t parse_raw_messages();

  void send(const Message& message);
  void send(const std::string& str);
};