#pragma once

#include "events.h"
#include "message_parser.h"
#include "talker.h"

#include <optional>
#include <deque>
#include <string>

class HandlersManager;

class Handler {
public:
  Handler(EventLoopPtr event_loop, int fd, TalkerPtr talker);
  ~Handler();

  void connect_poller_add(EventDescriptor);
  void connect_poller_remove(EventDescriptor);
  void connect_handlers_manager_remove(EventDescriptor);
  void start();

private:
  using Buffer = std::deque<char>;

  std::optional<int> _fd;
  TalkerPtr _talker;

  EventDescriptor _poller_add = UNDEFINED_EVENT;
  EventDescriptor _poller_remove = UNDEFINED_EVENT;
  EventDescriptor _handlers_manager_remove = UNDEFINED_EVENT;
  EventLoopPtr _event_loop;

  Buffer _read_buffer;
  Buffer _write_buffer;
  MessageParser<Buffer> _parser;

  EventDescriptor _handler_desciptor;

  void setup_poll(bool write = false);
  void close();

  void process();
  void read();

  void write();
  void send(const Message& message);
  void send(const std::string& str);
};
