#pragma once

#include <charconv>
#include <optional>
#include <string_view>
#include <string>

std::string random_hexstring(std::size_t length);

bool starts_with(std::string_view str, std::string_view what);

std::string_view skip(std::string_view str, std::string_view what);

std::optional<int> parseInt(const char* first, std::size_t size);
