#pragma once

#include "events.h"

#include <memory>
#include <optional>
#include <string>
#include <unordered_set>

struct ServerInfo {
  static ServerInfo build(std::size_t argc, char** argv);

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

  void connect_poller_add(EventDescriptor);
  void connect_poller_remove(EventDescriptor);
  void connect_handlers_manager_add(EventDescriptor);
  void start();

  ServerInfo& info();

private:
  ServerInfo _info;

  EventDescriptor _poller_add = UNDEFINED_EVENT;
  EventDescriptor _poller_remove = UNDEFINED_EVENT;
  EventDescriptor _handlers_manager_add = UNDEFINED_EVENT;
  EventLoopPtr _event_loop;

  std::optional<int> _server_fd;

  std::optional<int> accept();

  void close();
};
