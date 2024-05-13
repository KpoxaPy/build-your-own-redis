#pragma once

#include <charconv>
#include <cxxabi.h>
#include <optional>
#include <string_view>
#include <string>


std::string random_hexstring(std::size_t length);

bool starts_with(std::string_view str, std::string_view what);

std::string_view skip(std::string_view str, std::string_view what);

std::optional<int> parseInt(const char* first, std::size_t size);

std::string to_lower_case(std::string_view);

template <typename T>
std::string demangled() {
  int status;
  char* realname;

  realname = abi::__cxa_demangle(typeid(T).name(), NULL, NULL, &status);

  std::string result = realname;
  if (status) {
    result = "[demangler error: " + std::to_string(status) + "]";
  } else {
    result = realname;
  }
  std::free(realname);
  return result;
}