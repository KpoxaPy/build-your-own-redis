#include "replica.h"

Replica::Replica(EventLoopPtr event_loop) 
  : _event_loop(event_loop) {
}

void Replica::set_storage(StoragePtr storage) {
  this->_storage = storage;
}

void Replica::set_server(ServerPtr server) {
  this->_server = server;
}

void Replica::start() {
}
