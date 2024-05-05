#include "handlers_manager.h"

HandlerAddEvent::HandlerAddEvent(int fd)
  : fd(fd) {
}

HandlerRemoveEvent::HandlerRemoveEvent(int fd)
  : fd(fd) {
}

HandlersManager::HandlersManager(EventLoopPtr event_loop)
  : _event_loop(event_loop) {
  this->_add_listener = this->_event_loop->make_desciptor();
  this->_remove_listener = this->_event_loop->make_desciptor();
}

void HandlersManager::connect_poller_add(EventDescriptor descriptor) {
  this->_poller_add = descriptor;
}

void HandlersManager::connect_poller_remove(EventDescriptor descriptor) {
  this->_poller_remove = descriptor;
}

EventDescriptor HandlersManager::add_listener() {
  return this->_add_listener;
}

EventDescriptor HandlersManager::remove_listener() {
  return this->_remove_listener;
}

void HandlersManager::start() {
  this->_event_loop->listen(this->_add_listener, [this](const Event& event) {
    auto& add_event = static_cast<const HandlerAddEvent&>(event);
    this->add(add_event.fd);
  });

  this->_event_loop->listen(this->_remove_listener, [this](const Event& event) {
    auto& remove_event = static_cast<const HandlerRemoveEvent&>(event);
    this->remove(remove_event.fd);
  });
}

void HandlersManager::add(int fd) {
  if (this->_handlers.contains(fd)) {
    throw std::runtime_error("Re-adding client fd to handlers manager is not allowed!");
  }

  auto& handler = this->_handlers.try_emplace(fd, this->_event_loop, fd).first->second;

  handler.connect_poller_add(this->_poller_add);
  handler.connect_poller_remove(this->_poller_remove);
  handler.connect_handlers_manager_remove(this->remove_listener());
  handler.start();
}

void HandlersManager::remove(int fd) {
  this->_handlers.erase(fd);
}
