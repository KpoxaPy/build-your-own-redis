#include "debug.h"
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
    auto info = ServerInfo::build(argc, argv);
    DEBUG_LEVEL = info.debug_level;

    auto event_loop = EventLoop::make();

    auto storage = std::make_shared<Storage>(event_loop);
    auto poller = std::make_shared<Poller>(event_loop);
    auto handlers_manager = std::make_shared<HandlersManager>(event_loop);
    auto server = std::make_shared<Server>(event_loop, info);

    const bool is_slave = server->info().replication.role == "slave";

    ReplicaPtr replica;
    if (is_slave) {
      replica = std::make_shared<Replica>(event_loop);
      replica->set_storage(storage);
      replica->set_server(server);
    }

    storage->start();
    poller->start();

    if (replica) {
      replica->connect_poller_add(poller->add_listener());
      replica->connect_poller_remove(poller->remove_listener());
      replica->start();
    }

    handlers_manager->set_talker([server, storage]() {
      auto talker = std::make_shared<ServerTalker>();
      talker->set_storage(storage);
      talker->set_server(server);
      return talker;
    });
    handlers_manager->connect_poller_add(poller->add_listener());
    handlers_manager->connect_poller_remove(poller->remove_listener());
    handlers_manager->start();

    server->connect_poller_add(poller->add_listener());
    server->connect_poller_remove(poller->remove_listener());
    server->connect_handlers_manager_add(handlers_manager->add_listener());
    server->start();

    event_loop->start();

    return 0;
  } catch (std::exception& e) {
    std::cerr << "Terminate, main caught exception: " << std::endl << e.what() << std::endl;

    return 1;
  }
}
