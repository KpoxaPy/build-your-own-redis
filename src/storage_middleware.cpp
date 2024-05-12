#include "storage_middleware.h"

StorageMiddleware::StorageMiddleware(EventLoopPtr event_loop)
  : _event_loop(event_loop) {
}

void StorageMiddleware::set_storage(IStoragePtr storage) {
  this->_storage = storage;
}

void StorageMiddleware::set(std::string key, std::string value, std::optional<int> expire_ms) {
  this->_storage->set(std::move(key), std::move(value), std::move(expire_ms));
}

std::optional<std::string> StorageMiddleware::get(std::string key) {
  return this->_storage->get(std::move(key));
}

void StorageMiddleware::add_replica() {
}

void StorageMiddleware::remove_replica() {
}
