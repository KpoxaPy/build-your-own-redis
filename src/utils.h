#pragma once

#include <charconv>
#include <cstdint>
#include <cxxabi.h>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string_view>
#include <string>
#include <type_traits>

std::string random_hexstring(std::size_t length);

bool starts_with(std::string_view str, std::string_view what);

std::string_view skip(std::string_view str, std::string_view what);

std::optional<int> parseInt(std::string_view);
std::optional<int> parseInt(const char* first, std::size_t size);

std::optional<std::uint64_t> parseUInt64(std::string_view);
std::optional<std::uint64_t> parseUInt64(const char* first, std::size_t size);

std::string to_lower_case(std::string_view);
std::string to_upper_case(std::string_view);

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

template <typename... Args>
std::string print_args(Args&&... args) {
  std::ostringstream ss;

  ((ss << std::forward<Args>(args)), ...);

  return ss.str();
}

std::int16_t byteswap(std::int16_t value) noexcept;
std::uint32_t byteswap(std::uint32_t value) noexcept;
std::int32_t byteswap(std::int32_t value) noexcept;

std::string to_hex(const std::string& value);

template <typename T>
inline typename std::enable_if<std::is_integral<T>::value, std::string>::type
to_hex(const T& value) {
  std::ostringstream ss;
  ss << std::hex << std::uppercase << std::setfill('0')
     << std::setw(sizeof(T) * 2) << value;
  return ss.str();
}
