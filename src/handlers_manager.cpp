#include "handlers_manager.h"

#include "debug.h"

HandlersManager::HandlersManager(EventLoopPtr event_loop)
  : _event_loop(event_loop) {
  this->_slot_add = this->_event_loop->listen([this](int fd) {
    this->add(fd);
  });

  this->_slot_remove = this->_event_loop->listen([this](int fd) {
    this->remove(fd);
  });
}

void HandlersManager::set_talker(TalkerBuilder builder) {
  this->_talker_builder = std::move(builder);
}

void HandlersManager::connect_poller_add(SlotDescriptor<void> descriptor) {
  this->_poller_add = std::move(descriptor);
}

void HandlersManager::connect_poller_remove(SlotDescriptor<void> descriptor) {
  this->_poller_remove = std::move(descriptor);
}

SlotDescriptor<void> HandlersManager::add() {
  return this->_slot_add->descriptor();
}

SlotDescriptor<void> HandlersManager::remove() {
  return this->_slot_remove->descriptor();
}

void HandlersManager::add(int fd) {
  if (this->_handlers.contains(fd)) {
    throw std::runtime_error("Re-adding client fd to handlers manager is not allowed!");
  }

  auto& handler = this->_handlers.try_emplace(fd, this->_event_loop, fd, this->_talker_builder()).first->second;

  handler.connect_poller_add(this->_poller_add);
  handler.connect_poller_remove(this->_poller_remove);
  handler.connect_handlers_manager_remove(this->remove());

  if (DEBUG_LEVEL >= 2) std::cerr << "DEBUG handlers manager connects = " << this->_handlers.size() << std::endl;
}

void HandlersManager::remove(int fd) {
  this->_handlers.erase(fd);
}
