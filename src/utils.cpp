#include "utils.h"

#include <algorithm>
#include <random>

std::string random_hexstring(std::size_t length)
{
    const std::string CHARACTERS = "0123456789abcdef";

    std::random_device random_device;
    std::mt19937 generator(random_device());
    std::uniform_int_distribution<> distribution(0, CHARACTERS.size() - 1);

    std::string random_string;

    for (std::size_t i = 0; i < length; ++i)
    {
        random_string += CHARACTERS[distribution(generator)];
    }

    return random_string;
}

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

std::optional<int> parseInt(const std::string& str) {
  return parseInt(str.data(), str.size());
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

std::string to_lower_case(std::string_view view) {
  std::string result{view.begin(), view.end()};
  std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) { return std::tolower(ch); });
  return result;
}

std::string to_upper_case(std::string_view view) {
  std::string result{view.begin(), view.end()};
  std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) { return std::toupper(ch); });
  return result;
}
