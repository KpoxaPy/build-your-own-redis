#pragma once

#include "talker.h"

class ServerTalker : public Talker {
public:
  std::optional<Message> talk() override;
  std::optional<Message> talk(const Message&) override;
};