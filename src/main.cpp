#include "events.h"
#include "poller.h"
#include "replica.h"
#include "server.h"
#include "server_talker.h"
#include "storage.h"
#include "handlers_manager.h"

#include <iostream>

int main(int argc, char **argv) {
  try {
    auto event_loop = EventLoop::make();

    auto storage = std::make_shared<Storage>(event_loop);
    auto poller = std::make_shared<Poller>(event_loop);
    auto handlers_manager = std::make_shared<HandlersManager>(event_loop);
    auto server = std::make_shared<Server>(event_loop, ServerInfo::build(argc, argv));

    auto talkerBuilder = []() {
      return std::make_shared<ServerTalker>();
    };

    storage->start();
    poller->start();

    handlers_manager->set_talker(talkerBuilder);
    handlers_manager->connect_poller_add(poller->add_listener());
    handlers_manager->connect_poller_remove(poller->remove_listener());
    handlers_manager->start();

    server->connect_poller_add(poller->add_listener());
    server->connect_poller_remove(poller->remove_listener());
    server->connect_handlers_manager_add(handlers_manager->add_listener());
    server->start();

    ReplicaPtr replica;
    if (server->info().replication.role == "slave") {
      replica = Replica::make();
      replica->set_storage(storage);
      replica->set_server(server);
      replica->start(event_loop);
    }

    event_loop->start();

    return 0;
  } catch (std::exception& e) {
    std::cerr << "Terminate, main caught exception: " << std::endl << e.what() << std::endl;

    return 1;
  }
}
