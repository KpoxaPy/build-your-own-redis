#include "server.h"
#include "storage.h"
#include "utils.h"

#include <cstdlib>
#include <iostream>
#include <list>
#include <sstream>

constexpr const int DEFAULT_PORT = 6379;

int main(int argc, char **argv) {
  int port = DEFAULT_PORT;

  int arg_pos = 1;
  while (arg_pos < argc) {
    if (std::string("--port") == argv[arg_pos]) {
      if (arg_pos + 1 >= argc) {
        throw std::runtime_error("--port argument requires argument");
      }

      port = std::atoi(argv[arg_pos + 1]);
      arg_pos += 2;
    } else {
      std::ostringstream ss;
      ss << "unxepected argument: " << argv[arg_pos];
      throw std::runtime_error(ss.str());
    }
  }

  try {
    Storage storage;
    Server server(port, storage);
    std::list<Client> clients;

    while (true) {
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
    }

    return 0;
  } catch (std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;

    return 1;
  }
}
