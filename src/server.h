#pragma once

#include "client.h"
#include "storage.h"

#include <optional>
#include <string>
#include <unordered_set>

struct ServerInfo {
  struct Replication {
    std::string role;
    std::string master_replid;
    int master_repl_offset;

    std::string master_host;
    int master_port;

    std::string to_string() const;
  } replication;

  struct Server {
    int tcp_port;
  } server;

  std::string to_string(std::unordered_set<std::string>) const;
};

class Server {
public:
  Server(Storage& storage);
  ~Server();

  void start();
  std::optional<Client> accept();

  ServerInfo& info();

private:
  static constexpr int CONN_BACKLOG = 5;
  std::optional<int> _server_fd;
  Storage& _storage;
  ServerInfo _info;

  void close();
};
