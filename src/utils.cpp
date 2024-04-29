#include "utils.h"

bool starts_with(std::string_view str, std::string_view what) {
  if (what.size() > str.size()) {
    return false;
  }

  return std::string_view{str.begin(), str.begin() + what.size()}
    == what;
}

std::string_view skip(std::string_view str, std::string_view what) {
  if (what.size() > str.size()) {
    return str;
  }

  if (std::string_view{str.begin(), str.begin() + what.size()}
    == what)
  {
    return {str.begin() + what.size(), str.end()};
  } else {
    return str;
  }
}

std::optional<int> parseInt(const char* first, std::size_t size) {
  int value;

  if (size == 0) {
    return {};
  }

  if (*first == '+') {
    ++first;
    --size;
    
    if (size == 0) {
      return {};
    }
  }

  if (std::from_chars(first, first + size, value).ec == std::errc{}) {
    return value;
  }

  return {};
}
