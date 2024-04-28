#include "server.h"

#include <iostream>
#include <list>

int main(int argc, char **argv) {
  try {
    Server server;
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
