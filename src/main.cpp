#include "events.h"
#include "poller.h"
#include "server.h"
#include "storage.h"

#include <iostream>

int main(int argc, char **argv) {
  try {
    auto info = ServerInfo::build(argc, argv);
    auto event_loop = EventLoopManager::make();
    Storage storage;
    Poller poller;
    Server server(storage, std::move(info));

    poller.start(event_loop);
    server.start(event_loop);
    event_loop->start();

    return 0;
  } catch (std::exception& e) {
    std::cerr << "Terminate, main caught exception: " << std::endl << e.what() << std::endl;

    return 1;
  }
}
