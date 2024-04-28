#pragma once

#include <chrono>
#include <cstddef>
#include <iostream>
#include <optional>
#include <queue>
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
  using RawMessage = std::vector<std::byte>;
  friend std::ostream& operator<<(std::ostream&, Client::RawMessage);
  using RawMessageBuffer = RawMessage;

  std::optional<int> _client_fd;
  std::chrono::steady_clock::time_point _begin_time;
  RawMessageBuffer _buffer;
  std::queue<RawMessage> _raw_messages;

  void close();

  void read();
  void parse_raw_messages();

  void send(const std::string& str);
};