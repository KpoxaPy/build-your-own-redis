#include "storage.h"

#include <memory>

Value::Value(std::string data)
  : _data(data)
  , _create_time(Clock::now())
{
}

const std::string& Value::data() const {
  return this->_data;
}

void Value::setExpire(std::chrono::milliseconds ms) {
  this->_expire_time = Clock::now() + ms;
}

void Value::setExpireTime(Timepoint tp) {
  this->_expire_time = tp;
}

std::optional<Timepoint> Value::getExpire() const {
  return this->_expire_time;
}

Storage::Storage() {
}

void Storage::restore(std::string key, std::string value, std::optional<Timepoint> expire_time) {
  auto& stored_value = this->_storage[key] = value;

  if (expire_time) {
    stored_value.setExpireTime(expire_time.value());
  }
}

void Storage::set(std::string key, std::string value, std::optional<int> expire_ms) {
  auto& stored_value = this->_storage[key] = value;

  if (expire_ms) {
    stored_value.setExpire(std::chrono::milliseconds{expire_ms.value()});
  }
}

std::optional<std::string> Storage::get(std::string key) {
  auto it = this->_storage.find(key);
  if (it == this->_storage.end()) {
    return {};
  }
  auto& value = it->second;

  if (value.getExpire() && Clock::now() >= value.getExpire()) {
    this->_storage.erase(key);
    return {};
  }

  return value.data();
}

std::vector<std::string> Storage::keys(std::string_view selector) const {
  std::vector<std::string> keys;

  // ignoring selector for now, return all keys as selector=*

  for (const auto& [key, value]: this->_storage) {
    keys.emplace_back(key);
  }

  return keys;
}
