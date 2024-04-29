#pragma once

#include "client.h"
#include "storage.h"

#include <optional>

class Server {
public:
  Server(int port, Storage& storage);
  ~Server();

  std::optional<Client> accept();

private:
  static constexpr int CONN_BACKLOG = 5;
  std::optional<int> _server_fd;
  Storage& _storage;

  void close();
};
