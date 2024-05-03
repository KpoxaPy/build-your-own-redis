#pragma once

#include "events.h"
// #include "message.h"
#include "storage.h"

#include <optional>
#include <deque>
#include <string>

class HandlersManager;
class Message;

class Handler {
public:
  enum class ProcessStatus {
    Keep,
    Closed,
  };

  Handler(int fd, HandlersManager& manager);
  Handler(Handler&&);
  ~Handler();

  void start(EventLoopManagerPtr event_loop);
  // ProcessStatus process();


private:
  using RawMessageBuffer = std::deque<char>;

  std::optional<int> _client_fd;
  HandlersManager& _manager;
  EventLoopManagerPtr _event_loop;

  RawMessageBuffer _read_buffer;
  RawMessageBuffer _write_buffer;

  EventDescriptor _handler_desciptor;

  void setup_poll(bool write = false);
  void close();

  void read();
  // std::size_t parse_raw_messages();

  void write();
  // void send(const Message& message);
  void send(const std::string& str);
};
