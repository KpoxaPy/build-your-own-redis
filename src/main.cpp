#include "debug.h"
#include "events.h"
#include "poller.h"
#include "replica.h"
#include "server.h"
#include "server_talker.h"
#include "storage.h"
#include "handlers_manager.h"

#include <iostream>

class StorageMiddleware;

int main(int argc, char **argv) {
  try {
    auto info = ServerInfo::build(argc, argv);
    DEBUG_LEVEL = info.debug_level;

    auto event_loop = EventLoop::make();

    auto poller = std::make_shared<Poller>(event_loop);
    auto storage = std::make_shared<Storage>(event_loop);
    auto storage_middleware = std::make_shared<StorageMiddleware>(event_loop);
    auto handlers_manager = std::make_shared<HandlersManager>(event_loop);
    auto server = std::make_shared<Server>(event_loop, info);

    ReplicaPtr replica;
    if (server->is_replica()) {
      replica = std::make_shared<Replica>(event_loop);
      replica->set_server(server);
    }

    storage_middleware->connect_storage_command(storage->command());

    if (replica) {
      replica->connect_storage_command(storage_middleware->command());
      replica->connect_poller_add(poller->add());
      replica->connect_poller_remove(poller->remove());
    }

    handlers_manager->set_talker([event_loop, server, storage_middleware]() {
      auto talker = std::make_shared<ServerTalker>(event_loop);
      talker->set_server(server);

      talker->connect_storage_command(storage_middleware->command());
      talker->connect_replica_add(storage_middleware->add_replica());

      return talker;
    });
    handlers_manager->connect_poller_add(poller->add());
    handlers_manager->connect_poller_remove(poller->remove());

    server->connect_poller_add(poller->add());
    server->connect_poller_remove(poller->remove());
    server->connect_handlers_manager_add(handlers_manager->add());

    storage->start();
    storage_middleware->start();
    event_loop->start();

    return 0;
  } catch (std::exception& e) {
    std::cerr << "Terminate, main caught exception: " << std::endl << e.what() << std::endl;

    return 1;
  }
}
