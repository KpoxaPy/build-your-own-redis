#include "storage.h"

#include "utils.h"

#include <memory>

std::string to_string(StorageType type) {
  switch (type) {
    case StorageType::None: return "none";
    case StorageType::String: return "string";
    case StorageType::Stream: return "stream";
  }

  throw std::runtime_error("unknown type of StorageType");
}

std::string to_string(StreamErrorType type) {
  switch (type) {
    case StreamErrorType::None:
      return "ERR None";
    case StreamErrorType::MustBeNotZeroId:
      return "ERR The ID specified in XADD must be greater than 0-0";
    case StreamErrorType::MustBeMoreThanTop:
      return "ERR The ID specified in XADD is equal or smaller than the target stream top item";
    case StreamErrorType::WrongKeyType:
      return "WRONGTYPE Operation against a key holding the wrong kind of value";
  }

  throw std::runtime_error("unknown type of StreamErrorType");
}

StreamId::StreamId(std::size_t ms, std::size_t id)
  : ms(ms), id(id) {
}

StreamId::StreamId(std::string_view str) {
  if (str.empty()) {
    throw std::runtime_error("empty StreamId is not allowed");
  }

  if (str == "0") {
    return; // just 0-0 id
  }

  if (str == "*") {
    this->general_wildcard = true;
    return; // just * id
  }

  auto delim_pos = str.find('-');

  if (delim_pos == str.npos) {
    throw std::runtime_error(print_args("unexpected StreamId composition: ", str));
  }

  auto maybe_ms = parseInt({str.begin() , str.begin() + delim_pos});
  if (!maybe_ms) {
    throw std::runtime_error(print_args("unexpected first part of StreamId: should be number: ", str));
  }
  this->ms = maybe_ms.value();

  std::string_view id_part{str.begin() + delim_pos + 1, str.end()};

  if (id_part == "*") {
    this->id_wildcard = true;
    return;
  }

  auto maybe_id = parseInt(id_part);
  if (!maybe_id) {
    throw std::runtime_error(print_args("unexpected second part of StreamId: should be number: ", str));
  }
  this->id = maybe_id.value();
}

bool StreamId::operator<(const StreamId& other) const {
  return (this->ms < other.ms) or (this->ms == other.ms and this->id < other.id);
}

bool StreamId::is_null() const {
  return this->is_full_defined() and this->ms == 0 and this->id == 0;
}

bool StreamId::is_full_defined() const {
  return !this->general_wildcard and !this->id_wildcard;
}

std::string StreamId::to_string() const {
  if (this->general_wildcard) {
    return "*";
  }

  if (this->id_wildcard) {
    return std::to_string(this->ms) + "-*";
  }

  return std::to_string(this->ms) + "-" + std::to_string(this->id);
}

Value::Value() {
  this->_type = StorageType::None;
}

StorageType Value::type() const {
  return this->_type;
}

StringValue::StringValue(std::string data)
  : _data(data)
  , _create_time(Clock::now())
{
  this->_type = StorageType::String;
}

const std::string& StringValue::data() const {
  return this->_data;
}

void StringValue::setExpire(std::chrono::milliseconds ms) {
  this->_expire_time = Clock::now() + ms;
}

void StringValue::setExpireTime(Timepoint tp) {
  this->_expire_time = tp;
}

std::optional<Timepoint> StringValue::getExpire() const {
  return this->_expire_time;
}

std::tuple<StreamId, StreamErrorType> StreamValue::append(StreamId id, StreamPartValue values) {
  if (id.is_null()) {
    return {StreamId{}, StreamErrorType::MustBeNotZeroId};
  }

  if (this->_data.size() > 0) {
    auto last_id = this->_data.rbegin()->first;

    if (id.general_wildcard) {
      std::size_t next_ms = duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();
      if (next_ms <= last_id.ms) {
        // weirdo, last ms in future?, but maybe ok, just throw next id
        id = StreamId{last_id.ms, ++last_id.id};
      } else {
        id = StreamId{next_ms, 0};
      }
    } else if (id.id_wildcard) {
      if (id.ms < last_id.ms) {
        return {StreamId{}, StreamErrorType::MustBeMoreThanTop};
      } else if (id.ms == last_id.ms) {
        id = StreamId{id.ms, ++last_id.id};
      } else {
        id = StreamId{id.ms, 0};
      }
    } else if (id.ms < last_id.ms) {
      return {StreamId{}, StreamErrorType::MustBeMoreThanTop};
    } else if (id.ms == last_id.ms and id.id <= last_id.id) {
      return {StreamId{}, StreamErrorType::MustBeMoreThanTop};
    }
  } else if (id.general_wildcard) {
    id = StreamId{0, 1};
  } else if (id.id_wildcard) {
    if (id.ms == 0) {
      id = StreamId{0, 1};
    } else {
      id = StreamId{id.ms, 0};
    }
  }

  this->_data.emplace_hint(this->_data.end(), id, std::move(values));
  return {id, StreamErrorType::None};
}

Storage::Storage() {
}

void Storage::restore(std::string key, std::string value, std::optional<Timepoint> expire_time) {
  auto ptr = std::make_unique<StringValue>(value);
  if (expire_time) {
    ptr->setExpireTime(expire_time.value());
  }

  this->_storage.insert_or_assign(key, std::move(ptr));
}

void Storage::set(std::string key, std::string value, std::optional<int> expire_ms) {
  auto ptr = std::make_unique<StringValue>(value);
  if (expire_ms) {
    ptr->setExpire(std::chrono::milliseconds{expire_ms.value()});
  }

  this->_storage.insert_or_assign(key, std::move(ptr));
}

std::optional<std::string> Storage::get(std::string key) {
  auto it = this->_storage.find(key);
  if (it == this->_storage.end()) {
    return {};
  }
  auto& value = it->second;

  if (value->type() != StorageType::String) {
    return {};
  }

  auto& str = static_cast<StringValue&>(*value);

  if (str.getExpire() && Clock::now() >= str.getExpire()) {
    this->_storage.erase(key);
    return {};
  }

  return str.data();
}

std::tuple<StreamId, StreamErrorType> Storage::xadd(std::string key, StreamId id, StreamPartValue values) {
  auto it = this->_storage.find(key);
  if (it != this->_storage.end()) {
    if (it->second->type() != StorageType::Stream) {
      return {StreamId{}, StreamErrorType::WrongKeyType};
    }

    auto& stored = static_cast<StreamValue&>(*it->second);

    return stored.append(id, std::move(values));
  }

  auto ptr = std::make_unique<StreamValue>();
  auto result = ptr->append(id, std::move(values));
  if (std::get<1>(result) == StreamErrorType::None) {
    this->_storage.emplace(key, std::move(ptr));
  }
  return result;
}

StorageType Storage::type(std::string key) {
  auto it = this->_storage.find(key);
  if (it == this->_storage.end()) {
    return StorageType::None;
  }

  return StorageType::String;
}

std::vector<std::string> Storage::keys(std::string_view selector) const {
  std::vector<std::string> keys;

  // ignoring selector for now, return all keys as selector=*

  for (const auto& [key, value]: this->_storage) {
    keys.emplace_back(key);
  }

  return keys;
}
