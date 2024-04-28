#pragma once

#include "client.h"

#include <optional>

class Server {
  static constexpr int PORT = 6379;
  static constexpr int CONN_BACKLOG = 5;
  std::optional<int> _server_fd;

public:
  Server();
  ~Server();

  std::optional<Client> accept();

private:
  void close();
};
