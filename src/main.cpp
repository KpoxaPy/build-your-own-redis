#include "events.h"
#include "poller.h"
#include "server.h"
#include "storage.h"
#include "handlers_manager.h"

#include <iostream>

int main(int argc, char **argv) {
  try {
    auto event_loop = EventLoopManager::make();
    auto poller = Poller::make();
    auto storage = Storage::make();
    auto handlers_manager = HandlersManager::make();
    auto server = Server::make(ServerInfo::build(argc, argv));

    handlers_manager->set_server(server);
    handlers_manager->set_storage(storage);

    poller->start(event_loop);
    storage->start(event_loop);
    handlers_manager->start(event_loop);
    server->start(event_loop);

    event_loop->start();

    return 0;
  } catch (std::exception& e) {
    std::cerr << "Terminate, main caught exception: " << std::endl << e.what() << std::endl;

    return 1;
  }
}
