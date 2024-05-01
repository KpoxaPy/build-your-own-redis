#include "events.h"
#include "server.h"
#include "storage.h"
#include "utils.h"

#include <cstdlib>
#include <iostream>
#include <list>
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
    Server server(storage, std::move(info));
    std::list<Client> clients;

    server.start();

    event_loop->post([&server, &clients](){
      try {
        while (auto maybe_client = server.accept()) {
          clients.push_back(std::move(maybe_client.value()));
        }
      } catch (std::exception& e) {
        std::cerr << "Clients accepting error: " << e.what() << std::endl;
      }

      std::list<std::list<Client>::iterator> clients_to_cleanup;
      for (auto client_it = clients.begin(); client_it != clients.end(); ++client_it) {
        try {
          if (client_it->process() == Client::ProcessStatus::Closed) {
            clients_to_cleanup.push_back(client_it);
          }
        } catch (std::exception& e) {
          std::cerr << "Client processing error: " << e.what() << std::endl;
        }
      }

      for (auto& client_it: clients_to_cleanup) {
        clients.erase(client_it);
      }
    }, EventLoopManager::Repeat);
    event_loop->start_loop();

    return 0;
  } catch (std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;

    return 1;
  }
}
