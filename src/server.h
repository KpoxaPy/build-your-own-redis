#pragma once

#include "events.h"

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

  void connect_poller_add(SlotDescriptor<void>);
  void connect_poller_remove(SlotDescriptor<void>);
  void connect_handlers_manager_add(SlotDescriptor<void>);

  ServerInfo& info();
  bool is_replica() const;

private:
  ServerInfo _info;
  bool _is_replica;

  SlotDescriptor<void> _poller_add;
  SlotDescriptor<void> _poller_remove;
  SlotDescriptor<void> _handlers_manager_add;

  SlotHolderPtr<void> _slot_accept;

  EventLoopPtr _event_loop;

  std::optional<int> _server_fd;

  void start();

  std::optional<int> accept();

  void close();
};
