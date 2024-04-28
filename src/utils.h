#pragma once

#include <string_view>

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