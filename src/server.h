#pragma once

#include "client.h"
#include "storage.h"

#include <optional>

class Server {
public:
  Server(Storage& storage);
  ~Server();

  std::optional<Client> accept();

private:
  static constexpr int PORT = 6379;
  static constexpr int CONN_BACKLOG = 5;
  std::optional<int> _server_fd;
  Storage& _storage;

  void close();
};
