#pragma once

#include "message.h"

#include <memory>
#include <optional>

class Talker {
public:
  virtual std::optional<Message> talk() = 0;
  virtual std::optional<Message> talk(const Message&) = 0;

  virtual Message::Type expected() = 0;
};
using TalkerPtr = std::shared_ptr<Talker>;