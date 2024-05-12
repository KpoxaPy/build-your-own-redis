#pragma once

#include "events.h"
#include "poller.h"
#include "signal_slot.h"

#include <memory>
#include <optional>
#include <string>
#include <unordered_set>

constexpr int DEFAULT_DEBUG_LEVEL = 0;

struct ServerInfo {
  static ServerInfo build(std::size_t argc, char** argv);

  int debug_level = DEFAULT_DEBUG_LEVEL;

  struct Server {
    int tcp_port;

    std::string to_string() const;
  } server;

  struct Replication {
    std::string role;
    std::string master_replid;
    int master_repl_offset;

    std::string master_host;
    int master_port;

    std::string to_string() const;
  } replication;

  std::string to_string(std::unordered_set<std::string>) const;
};

class Server;
using ServerPtr = std::shared_ptr<Server>;
class Server {
public:
  Server(EventLoopPtr event_loop, ServerInfo info = {});
  ~Server();

  SignalPtr<int, PollEventTypeList, SignalPtr<PollEventType>>& new_server_fd();
  SignalPtr<int>& removed_server_fd();
  SignalPtr<int>& new_fd();
  SignalPtr<int>& removed_fd();

  ServerInfo& info();
  bool is_replica() const;

private:
  ServerInfo _info;
  bool _is_replica;

  SignalPtr<int, PollEventTypeList, SignalPtr<PollEventType>> _new_server_fd_signal;
  SignalPtr<int> _removed_server_fd_signal;
  SignalPtr<int> _new_fd_signal;
  SignalPtr<int> _removed_fd_signal;

  SignalPtr<PollEventType> _fd_event_signal;
  SlotPtr<PollEventType> _slot_fd_event;

  EventLoopPtr _event_loop;

  std::optional<int> _server_fd;

  void start();

  std::optional<int> accept();

  void close();
};
