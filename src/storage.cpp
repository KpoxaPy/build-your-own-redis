#include "storage.h"

void Storage::set(std::string key, std::string value) {
  this->_storage[key] = value;
}

std::optional<std::reference_wrapper<const std::string>> Storage::get(std::string key) const {
  auto it = this->_storage.find(key);
  if (it != this->_storage.end()) {
    return {it->second};
  }
  return {};
}
