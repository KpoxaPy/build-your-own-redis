#include "handlers_manager.h"

#include "debug.h"

HandlersManager::HandlersManager(EventLoopPtr event_loop)
  : _event_loop(event_loop) {
  this->_slot_add = std::make_shared<Slot<int>>([this](int fd) {
    this->add(fd);
  });

  this->_slot_remove = std::make_shared<Slot<int>>([this](int fd) {
    this->remove(fd);
  });
}

void HandlersManager::set_talker(TalkerBuilder builder) {
  this->_talker_builder = std::move(builder);
}

SlotPtr<int>& HandlersManager::add_fd() {
  return this->_slot_add;
}

SlotPtr<int>& HandlersManager::remove_fd() {
  return this->_slot_remove;
}

SignalPtr<int, PollEventTypeList, SignalPtr<PollEventType>>& HandlersManager::new_fd() {
  return this->_new_fd_signal;
}

SignalPtr<int>& HandlersManager::removed_fd() {
  return this->_removed_fd_signal;
}

void HandlersManager::add(int fd) {
  if (this->_handlers.contains(fd)) {
    throw std::runtime_error("Re-adding client fd to handlers manager is not allowed!");
  }

  auto& handler = this->_handlers.try_emplace(fd, this->_event_loop, fd, this->_talker_builder()).first->second;

  handler.new_fd()->connect(this->_new_fd_signal);
  handler.removed_fd()->connect(this->_removed_fd_signal);
  handler.removed_fd()->connect(this->_slot_remove);

  if (DEBUG_LEVEL >= 2) std::cerr << "DEBUG handlers manager connects = " << this->_handlers.size() << std::endl;
}

void HandlersManager::remove(int fd) {
  this->_handlers.erase(fd);
}
