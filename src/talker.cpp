#include "talker.h"

std::optional<Message> Talker::say() {
  if (this->_pending.empty()) {
    return {};
  }

  auto message = std::move(this->_pending.front());
  this->_pending.pop_front();

  return {std::move(message)};
}
