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

std::optional<Timepoint> Value::getExpire() const {
  return this->_expire_time;
}

Storage::Storage(EventLoopPtr event_loop)
  : _event_loop(event_loop) {
}

void Storage::start() {
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
