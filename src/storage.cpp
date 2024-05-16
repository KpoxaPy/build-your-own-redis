#include "storage.h"

#include "utils.h"

#include <algorithm>
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

StreamRange::StreamRange(Iterator begin, Iterator end)
    : _begin(begin), _end(end) {
}

StreamRange::Iterator StreamRange::begin() const {
  return this->_begin;
}

StreamRange::Iterator StreamRange::end() const {
  return this->_end;
}

StreamId::StreamId(std::size_t ms, std::size_t id)
  : ms(ms), id(id) {
}

StreamId::StreamId(std::string_view str) {
  this->from_string(str);
}

void StreamId::from_string(std::string_view str) {
  if (str.empty()) {
    throw StreamIdParseError("empty StreamId is not allowed");
  }

  if (str == "0") {
    return; // just 0-0 id
  }

  auto delim_pos = str.find('-');

  bool seek_id = true;
  if (delim_pos == str.npos) {
    seek_id = false;
    delim_pos = str.size();
  }

  auto maybe_ms = parseUInt64({str.begin() , str.begin() + delim_pos});
  if (!maybe_ms) {
    throw StreamIdParseError(print_args("unexpected first part of StreamId: should be number: ", str));
  }
  this->ms = maybe_ms.value();

  if (seek_id) {
    auto maybe_id = parseUInt64({str.begin() + delim_pos + 1, str.end()});
    if (!maybe_id) {
      throw StreamIdParseError(print_args("unexpected second part of StreamId: should be number: ", str));
    }
    this->id = maybe_id.value();
  }
}

bool StreamId::operator<(const StreamId& other) const {
  return (this->ms < other.ms) or (this->ms == other.ms and this->id < other.id);
}

bool StreamId::is_null() const {
  return this->ms == 0 and this->id == 0;
}

InputStreamId::InputStreamId(std::string_view str) {
  this->from_string(str);
}

bool InputStreamId::is_full_defined() const {
  return !this->general_wildcard and !this->id_wildcard;
}

bool InputStreamId::is_null() const {
  return this->is_full_defined() and StreamId::is_null();
}

void InputStreamId::from_string(std::string_view str) {
  if (str == "0") {
    return; // just 0-0 id
  }

  if (str == "*") {
    this->general_wildcard = true;
    return; // just * id
  }

  auto delim_pos = str.find('-');

  if (delim_pos == str.npos) {
    throw StreamIdParseError(print_args("unexpected StreamId composition: ", str));
  }

  auto maybe_ms = parseUInt64({str.begin() , str.begin() + delim_pos});
  if (!maybe_ms) {
    throw StreamIdParseError(print_args("unexpected first part of StreamId: should be number: ", str));
  }
  this->ms = maybe_ms.value();

  std::string_view id_part{str.begin() + delim_pos + 1, str.end()};

  if (id_part == "*") {
    this->id_wildcard = true;
    return;
  }

  auto maybe_id = parseUInt64(id_part);
  if (!maybe_id) {
    throw StreamIdParseError(print_args("unexpected second part of StreamId: should be number: ", str));
  }
  this->id = maybe_id.value();
}

std::string InputStreamId::to_string() const {
  if (this->general_wildcard) {
    return "*";
  }

  if (this->id_wildcard) {
    return std::to_string(this->ms) + "-*";
  }

  return StreamId::to_string();
}

BoundStreamId::BoundStreamId(std::string_view str) {
  this->from_string(str);
}

void BoundStreamId::from_string(std::string_view str) {
  if (str == "-") {
    this->is_left_unbound = true;
    return;
  } else if (str == "+") {
    this->is_right_unbound = true;
    return;
  }

  StreamId::from_string(str);
}

std::string BoundStreamId::to_string() const {
  if (this->is_left_unbound) {
    return "-";
  } else if (this->is_right_unbound) {
    return "+";
  }
  return StreamId::to_string();
}

std::string StreamId::to_string() const {
  return std::to_string(this->ms) + "-" + std::to_string(this->id);
}

StreamIdParseError::StreamIdParseError(std::string reason)
  : std::runtime_error(reason)
{
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

StreamValue::StreamValue() {
  this->_type = StorageType::Stream;
}

std::tuple<StreamId, StreamErrorType> StreamValue::append(InputStreamId in_id, StreamPartValue values) {
  if (in_id.is_null()) {
    return {StreamId{}, StreamErrorType::MustBeNotZeroId};
  }

  StreamId id;
  if (this->_data.size() > 0) {
    auto last_id = this->_data.rbegin()->first;

    if (in_id.general_wildcard) {
      std::size_t next_ms = duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();
      if (next_ms <= last_id.ms) {
        // weirdo, last ms in future?, but maybe ok, just throw next id
        id = StreamId{last_id.ms, ++last_id.id};
      } else {
        id = StreamId{next_ms, 0};
      }
    } else if (in_id.id_wildcard) {
      if (in_id.ms < last_id.ms) {
        return {StreamId{}, StreamErrorType::MustBeMoreThanTop};
      } else if (in_id.ms == last_id.ms) {
        id = StreamId{in_id.ms, ++last_id.id};
      } else {
        id = StreamId{in_id.ms, 0};
      }
    } else if (in_id.ms < last_id.ms) {
      return {StreamId{}, StreamErrorType::MustBeMoreThanTop};
    } else if (in_id.ms == last_id.ms and in_id.id <= last_id.id) {
      return {StreamId{}, StreamErrorType::MustBeMoreThanTop};
    } else {
      id = StreamId{in_id.ms, in_id.id};
    }
  } else if (in_id.general_wildcard) {
    std::size_t next_ms = duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();
    id = StreamId{next_ms, 0};
  } else if (in_id.id_wildcard) {
    if (in_id.ms == 0) {
      id = StreamId{0, 1};
    } else {
      id = StreamId{in_id.ms, 0};
    }
  } else {
    id = StreamId{in_id.ms, in_id.id};
  }

  this->_data.emplace_hint(this->_data.end(), id, std::move(values));
  return {id, StreamErrorType::None};
}

StreamRange StreamValue::xrange(BoundStreamId left_id, BoundStreamId right_id) {
  StreamRange::Iterator begin_it;
  StreamRange::Iterator end_it;

  if (left_id.is_left_unbound) {
    begin_it = this->_data.cbegin();
  } else {
    begin_it = this->_data.lower_bound(static_cast<StreamId>(left_id));
  }

  if (right_id.is_right_unbound) {
    end_it = this->_data.cend();
  } else {
    end_it = this->_data.upper_bound(static_cast<StreamId>(right_id));
  }

  return {begin_it, end_it};
}

StreamRange StreamValue::xread(StreamId id) {
  auto begin_it = this->_data.lower_bound(static_cast<StreamId>(id));

  if (begin_it == this->_data.end()) {
    return {};
  }

  return {++begin_it, this->_data.end()};
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

std::tuple<StreamId, StreamErrorType> Storage::xadd(std::string key, InputStreamId id, StreamPartValue values) {
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

StreamRange Storage::xrange(std::string key, BoundStreamId left_id, BoundStreamId right_id) {
  auto it = this->_storage.find(key);
  if (it == this->_storage.end()) {
    return {};
  }

  auto& stored = static_cast<StreamValue&>(*it->second);
  return stored.xrange(left_id, right_id);
}

StreamsReadResult Storage::xread(StreamsReadRequest request) {
  StreamsReadResult result;

  for (const auto& [key, id]: request) {
    auto it = this->_storage.find(key);
    if (it == this->_storage.end()) {
      result.emplace_back(key, StreamRange{});
      continue;
    }

    auto& stored = static_cast<StreamValue&>(*it->second);
    result.emplace_back(key, stored.xread(id));
  }

  return result;
}

StorageType Storage::type(std::string key) {
  auto it = this->_storage.find(key);
  if (it == this->_storage.end()) {
    return StorageType::None;
  }

  return it->second->type();
}

std::vector<std::string> Storage::keys(std::string_view selector) const {
  std::vector<std::string> keys;

  // ignoring selector for now, return all keys as selector=*

  for (const auto& [key, value]: this->_storage) {
    keys.emplace_back(key);
  }

  return keys;
}
