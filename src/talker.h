#pragma once

#include "command.h"
#include "message.h"

#include <deque>
#include <memory>
#include <optional>
#include <type_traits>

class Talker {
public:
  virtual void listen(Message message) = 0;
  virtual std::optional<Message> say();

  virtual Message::Type expected() = 0;

protected:
  std::deque<Message> _pending;

  template <typename... Args>
  inline void next_say(Args&&... args) {
    this->_pending.emplace_back(std::forward<Args>(args)...);
  }

  template <
      typename T,
      typename... Args,
      typename = std::enable_if<std::is_base_of<Command, T>::value>>
  inline void next_say(Args&&... args) {
    this->_pending.push_back(T(std::forward<Args>(args)...).construct());
  }
};
using TalkerPtr = std::shared_ptr<Talker>;
