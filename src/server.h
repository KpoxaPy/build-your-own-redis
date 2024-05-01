#pragma once

#include "events.h"
#include "handler.h"
#include "storage.h"

#include <list>
#include <optional>
#include <string>
#include <unordered_set>

struct ServerInfo {
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

class Server {
public:
  Server(Storage& storage, ServerInfo info = {});
  ~Server();

  void start(EventLoopManagerPtr event_loop);
  std::optional<Handler> accept();

  ServerInfo& info();

private:
  Storage& _storage;
  ServerInfo _info;

  EventLoopManagerPtr _event_loop;
  std::optional<int> _server_fd;

  std::list<Handler> _handlers;

  void close();
};
