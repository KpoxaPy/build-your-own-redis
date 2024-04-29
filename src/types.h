#pragma once

#include <deque>
#include <iostream>
#include <vector>

using RawMessage = std::vector<std::byte>;
using RawMessageBuffer = RawMessage;
using RawMessagesStream = std::deque<RawMessage>;
std::ostream& operator<<(std::ostream&, RawMessage);
