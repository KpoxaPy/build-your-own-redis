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

StoragePtr Storage::make() {
  return std::make_shared<Storage>();
}

void Storage::start(EventLoopManagerPtr event_loop) {
  this->_event_loop = event_loop;
}
