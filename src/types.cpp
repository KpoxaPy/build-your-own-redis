#include "types.h"

std::ostream& operator<<(std::ostream& stream, RawMessage raw_message) {
  for (const auto& b: raw_message) {
    stream << static_cast<char>(b);
  }
  return stream;
}