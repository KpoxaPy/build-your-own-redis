#pragma once

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

class Storage {
public:
  void set(std::string key, std::string value);
  std::optional<std::reference_wrapper<const std::string>> get(std::string key) const;

private:
  std::unordered_map<std::string, std::string> _storage;
};