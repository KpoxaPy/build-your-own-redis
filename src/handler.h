#pragma once

#include "events.h"
#include "message_parser.h"
#include "poller.h"
#include "signal_slot.h"
#include "talker.h"

#include <optional>
#include <deque>
#include <string>

class HandlersManager;

class Handler {
public:
  Handler(EventLoopPtr event_loop, int fd, TalkerPtr talker);
  ~Handler();

  SignalPtr<int, PollEventTypeList, SignalPtr<PollEventType>>& new_fd();
  SignalPtr<int>& removed_fd();

private:
  using Buffer = std::deque<char>;

  std::optional<int> _fd;
  TalkerPtr _talker;

  SignalPtr<int, PollEventTypeList, SignalPtr<PollEventType>> _new_fd_signal;
  SignalPtr<int> _removed_fd_signal;
  SignalPtr<PollEventType> _fd_event_signal;
  SlotPtr<PollEventType> _slot_fd_event;
  EventLoopPtr _event_loop;

  Buffer _read_buffer;
  Buffer _write_buffer;
  MessageParser<Buffer> _parser;

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
