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

  void connect_poller_add(SlotDescriptor<void>);
  void connect_poller_remove(SlotDescriptor<void>);
  void connect_handlers_manager_remove(SlotDescriptor<void>);

private:
  using Buffer = std::deque<char>;

  std::optional<int> _fd;
  TalkerPtr _talker;

  SlotDescriptor<void> _poller_add;
  SlotDescriptor<void> _poller_remove;
  SlotDescriptor<void> _handlers_manager_remove;
  EventLoopPtr _event_loop;

  Buffer _read_buffer;
  Buffer _write_buffer;
  MessageParser<Buffer> _parser;

  SlotHolderPtr<void> _slot_handle;

  void start();

  void setup_poll(bool write = false);
  void close();

  void process_read();
  void process_write();

  void read();
  void write();

  void send(const Message& message);
  void send(const std::string& str);
};
