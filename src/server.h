#pragma once

#include "events.h"
#include "storage.h"

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
  template <typename ...Args>
  static ServerPtr make(Args&& ...args) {
    return std::make_shared<Server>(std::forward<Args&&>(args)...);
  }

  Server(ServerInfo info = {});
  ~Server();

  void start(EventLoopManagerPtr event_loop);

  ServerInfo& info();

private:
  ServerInfo _info;

  EventLoopManagerPtr _event_loop;
  std::optional<int> _server_fd;

  std::optional<int> accept();

  void close();
};
