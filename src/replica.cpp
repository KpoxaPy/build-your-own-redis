#include "replica.h"

ReplicaPtr Replica::make() {
  return std::make_shared<Replica>();
}

void Replica::set_storage(StoragePtr storage) {
  this->_storage = storage;
}

void Replica::set_server(ServerPtr server) {
  this->_server = server;
}

void Replica::start(EventLoopManagerPtr event_loop) {
  this->_event_loop = event_loop;
}
