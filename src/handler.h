#pragma once

#include "events.h"
#include "message_parser.h"
#include "command.h"

#include <optional>
#include <deque>
#include <string>

class HandlersManager;

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

private:
  using Buffer = std::deque<char>;

  std::optional<int> _client_fd;
  HandlersManager& _manager;
  EventLoopManagerPtr _event_loop;

  Buffer _read_buffer;
  Buffer _write_buffer;
  MessageParser<Buffer> _parser;

  EventDescriptor _handler_desciptor;

  void setup_poll(bool write = false);
  void close();

  void process();
  void read();
  void reply(CommandPtr);

  void write();
  void send(const Message& message);
  void send(const std::string& str);
};
