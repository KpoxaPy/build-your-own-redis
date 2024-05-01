#pragma once

#include "message.h"
#include "storage.h"
#include "types.h"

#include <chrono>
#include <cstddef>
#include <iostream>
#include <optional>
#include <deque>
#include <string>
#include <vector>

class Server;

class Handler {
public:
  enum class ProcessStatus {
    Keep,
    Closed,
  };

  Handler(int fd, Server& server, Storage& storage);
  Handler(Handler&&);
  ~Handler();

  Server& server();
  Storage& storage();

  ProcessStatus process();

  void send(const Message& message);

private:
  std::optional<int> _client_fd;
  std::chrono::steady_clock::time_point _begin_time;
  RawMessageBuffer _buffer;
  RawMessagesStream _raw_messages;
  Server& _server;
  Storage& _storage;

  void close();

  std::size_t read();
  std::size_t parse_raw_messages();

  void send(const std::string& str);
};