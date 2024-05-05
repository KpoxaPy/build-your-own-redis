#include "handlers_manager.h"

#include <memory>

HandlersMangerPtr HandlersManager::make() {
  return std::make_shared<HandlersManager>();
}

void HandlersManager::start(EventLoopManagerPtr event_loop) {
  this->_event_loop = event_loop;

  this->_event_loop->listen(EventHandlerAdd, [this](const Event& event) {
    auto& add_event = static_cast<const HandlerAddEvent&>(event);
    this->add(add_event.fd);
  });

  this->_event_loop->listen(EventHandlerRemove, [this](const Event& event) {
    auto& remove_event = static_cast<const HandlerRemoveEvent&>(event);
    this->remove(remove_event.fd);
  });
}

void HandlersManager::add(int fd) {
  if (this->_handlers.contains(fd)) {
    throw std::runtime_error("Re-adding client fd to handlers manager is not allowed!");
  }

  auto& handler = this->_handlers.try_emplace(fd, fd, *this).first->second;
  handler.start(this->_event_loop);
}

void HandlersManager::remove(int fd) {
  this->_handlers.erase(fd);
}
