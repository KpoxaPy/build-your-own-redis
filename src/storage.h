#pragma once

#include "events.h"
#include "rdb_parser.h"

#include <chrono>
#include <exception>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string_view>
#include <string>
#include <unordered_map>

enum class StorageType {
  None,
  String,
  Stream,
};

std::string to_string(StorageType type);

enum class StreamErrorType {
  None,
  MustBeNotZeroId,
  MustBeMoreThanTop,
  WrongKeyType,
};

std::string to_string(StreamErrorType type);

struct StreamId {
  std::size_t ms = 0;
  std::size_t id = 0;

  StreamId(std::size_t ms = 0, std::size_t id = 0);
  StreamId(std::string_view);

  bool operator==(const StreamId&) const;
  bool operator<(const StreamId&) const;

  bool is_null() const;

  void from_string(std::string_view);
  std::string to_string() const;
};

struct InputStreamId : public StreamId {
  bool id_wildcard = false;
  bool general_wildcard = false;

  InputStreamId() = default;
  InputStreamId(std::string_view);

  bool is_full_defined() const;
  bool is_null() const;

  void from_string(std::string_view);
  std::string to_string() const;
};

struct BoundStreamId : public StreamId {
  bool is_left_unbound = false;
  bool is_right_unbound = false;

  BoundStreamId() = default;
  BoundStreamId(std::string_view);

  void from_string(std::string_view);
  std::string to_string() const;
};

struct ReadStreamId : public StreamId {
  bool is_next_expected = false;

  ReadStreamId() = default;
  ReadStreamId(std::string_view);

  void from_string(std::string_view);
  std::string to_string() const;
};

class StreamIdParseError : public std::runtime_error {
public:
  StreamIdParseError(std::string);
};

using StreamPartValue = std::vector<std::pair<std::string, std::string>>;
using StreamDataType = std::map<StreamId, StreamPartValue>;
class StreamRange {
public:
  using Iterator = StreamDataType::const_iterator;

  StreamRange() = default;
  StreamRange(Iterator begin, Iterator end);

  Iterator begin() const;
  Iterator end() const;

private:
  Iterator _begin;
  Iterator _end;
};

using StreamsReadRequest = std::vector<std::pair<std::string, ReadStreamId>>;
using StreamsReadResult = std::vector<std::pair<std::string, StreamRange>>;

class IStorage : public IRDBParserListener {
public:
  virtual ~IStorage() = default;

  virtual void set(std::string key, std::string value, std::optional<int> expire_ms) = 0;
  virtual std::optional<std::string> get(std::string key) = 0;

  virtual std::tuple<StreamId, StreamErrorType> xadd(std::string key, InputStreamId id, StreamPartValue values) = 0;
  virtual StreamRange xrange(std::string key, BoundStreamId left_id, BoundStreamId right_id) = 0;
  virtual void xread(StreamsReadRequest, std::optional<std::size_t> block_ms, std::function<void(StreamsReadResult)> callback) = 0;

  virtual StorageType type(std::string key) = 0;

  virtual std::vector<std::string> keys(std::string_view selector) const = 0;

};
using IStoragePtr = std::shared_ptr<IStorage>;

class Value {
public:
  Value();

  StorageType type() const;

protected:
  StorageType _type;
};
using ValuePtr = std::unique_ptr<Value>;

class StringValue : public Value {
public:
  StringValue(std::string data = "");

  const std::string& data() const;

  void setExpire(std::chrono::milliseconds ms);
  void setExpireTime(Timepoint tp);
  std::optional<Timepoint> getExpire() const;

private:
  std::string _data;
  Timepoint _create_time;
  std::optional<Timepoint> _expire_time;
};

class StreamValue : public Value {
public:
  StreamValue();

  std::tuple<StreamId, StreamErrorType> append(InputStreamId, StreamPartValue values);

  StreamRange xrange(BoundStreamId left_id, BoundStreamId right_id);
  StreamRange xread(ReadStreamId id);

  StreamId last_id();

private:
  StreamDataType _data;
};

class Storage : public IStorage {
  struct WaitHandle;
  using WaitHandlePtr = std::shared_ptr<WaitHandle>;
  using WaitList = std::list<WaitHandlePtr>;

  struct WaitHandle : public std::enable_shared_from_this<WaitHandle> {
    WaitHandle(Storage&, StreamsReadRequest, std::size_t timeout_ms, std::function<void(StreamsReadResult)>);

    Storage& parent;

    std::size_t timeout_ms;
    EventLoop::JobHandle timeout;

    StreamsReadRequest request;
    std::function<void(StreamsReadResult)> callback;

    std::unordered_map<std::string, WaitList::iterator> stream_its;

    void setup();
    void reply();
  };

public:
  Storage(EventLoopPtr event_loop);

  void restore(std::string key, std::string value, std::optional<Timepoint> expire_time) override;

  void set(std::string key, std::string value, std::optional<int> expire_ms) override;
  std::optional<std::string> get(std::string key) override;

  std::tuple<StreamId, StreamErrorType> xadd(std::string key, InputStreamId id, StreamPartValue values) override;
  StreamRange xrange(std::string key, BoundStreamId left_id, BoundStreamId right_id) override;
  void xread(StreamsReadRequest, std::optional<std::size_t> block_ms, std::function<void(StreamsReadResult)> callback) override;

  StorageType type(std::string key) override;

  std::vector<std::string> keys(std::string_view selector) const override;

private:
  EventLoopPtr _event_loop;

  std::unordered_map<std::string, ValuePtr> _storage;

  std::unordered_map<std::string, WaitList> _stream_waitlists;
};
