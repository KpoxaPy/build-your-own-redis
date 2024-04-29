#pragma once

#include <charconv>
#include <optional>
#include <string_view>

bool starts_with(std::string_view str, std::string_view what);

std::string_view skip(std::string_view str, std::string_view what);

std::optional<int> parseInt(const char* first, std::size_t size);
