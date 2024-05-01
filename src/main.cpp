#include "events.h"
#include "poller.h"
#include "server.h"
#include "storage.h"
#include "utils.h"

#include <cstdlib>
#include <iostream>
#include <sstream>

constexpr const int DEFAULT_PORT = 6379;
ServerInfo setup_info(int argc, char **argv) {
  ServerInfo info;

  info.server.tcp_port = DEFAULT_PORT;
  info.replication.role = "master";
  info.replication.master_replid = random_hexstring(40);
  info.replication.master_repl_offset = 0;

  int arg_pos = 1;
  while (arg_pos < argc) {
    if (std::string("--port") == argv[arg_pos]) {
      if (arg_pos + 1 >= argc) {
        throw std::runtime_error("--port argument requires argument");
      }

      info.server.tcp_port = std::atoi(argv[arg_pos + 1]);
      arg_pos += 2;
    } else if (std::string("--replicaof") == argv[arg_pos]) {
      if (arg_pos + 2 >= argc) {
        throw std::runtime_error("--replicaof argument requires 2 arguments");
      }
      
      info.replication.role = "slave";
      info.replication.master_host = argv[arg_pos + 1];
      info.replication.master_port = std::atoi(argv[arg_pos + 2]);
      arg_pos += 3;
    } else {
      std::ostringstream ss;
      ss << "unexpected argument: " << argv[arg_pos];
      throw std::runtime_error(ss.str());
    }
  }

  return info;
}

int main(int argc, char **argv) {
  try {
    auto info = setup_info(argc, argv);
    auto event_loop = EventLoopManager::make();
    Storage storage;
    Poller poller;
    Server server(storage, std::move(info));

    poller.start(event_loop);
    server.start(event_loop);
    event_loop->start_loop();

    return 0;
  } catch (std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;

    return 1;
  }
}
