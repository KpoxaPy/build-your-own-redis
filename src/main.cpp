#include "debug.h"
#include "events.h"
#include "poller.h"
#include "replica.h"
#include "server.h"
#include "server_talker.h"
#include "storage.h"
#include "storage_middleware.h"
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
      replica->set_storage(storage_middleware);
      replica->new_fd()->connect(poller->add_fd());
      replica->removed_fd()->connect(poller->remove_fd());
    }

    storage_middleware->set_storage(storage);

    handlers_manager->set_talker([event_loop, server, storage_middleware]() {
      auto talker = std::make_shared<ServerTalker>(event_loop);
      talker->set_server(server);
      talker->set_storage(storage_middleware);
      talker->set_replicas_manager(storage_middleware);
      return talker;
    });
    handlers_manager->new_fd()->connect(poller->add_fd());
    handlers_manager->removed_fd()->connect(poller->remove_fd());

    server->new_server_fd()->connect(poller->add_fd());
    server->removed_server_fd()->connect(poller->remove_fd());
    server->new_fd()->connect(handlers_manager->add_fd());
    server->removed_fd()->connect(handlers_manager->remove_fd());

    event_loop->start();

    return 0;
  } catch (std::exception& e) {
    std::cerr << "Terminate, main caught exception: " << std::endl << e.what() << std::endl;

    return 1;
  }
}
