#pragma once

#include "rdb_parser.h"

#include <chrono>
#include <exception>
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

using StreamPartValue = std::vector<std::pair<std::string, std::string>>;

struct StreamId {
  std::size_t ms = 0;
  std::size_t id = 0;

  bool id_wildcard = false;
  bool general_wildcard = false;

  StreamId(std::size_t ms = 0, std::size_t id = 0);
  StreamId(std::string_view);

  bool operator<(const StreamId&) const;

  bool is_null() const;
  bool is_full_defined() const;

  std::string to_string() const;
};

class IStorage : public IRDBParserListener {
public:
  virtual ~IStorage() = default;

  virtual void set(std::string key, std::string value, std::optional<int> expire_ms) = 0;
  virtual std::optional<std::string> get(std::string key) = 0;

  virtual std::tuple<StreamId, StreamErrorType> xadd(std::string key, StreamId id, StreamPartValue values) = 0;

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
  std::tuple<StreamId, StreamErrorType> append(StreamId, StreamPartValue values);

private:
  std::map<StreamId, StreamPartValue> _data;
};

class Storage : public IStorage {
public:
  Storage();

  void restore(std::string key, std::string value, std::optional<Timepoint> expire_time) override;

  void set(std::string key, std::string value, std::optional<int> expire_ms) override;
  std::optional<std::string> get(std::string key) override;

  std::tuple<StreamId, StreamErrorType> xadd(std::string key, StreamId id, StreamPartValue values) override;

  StorageType type(std::string key) override;

  std::vector<std::string> keys(std::string_view selector) const override;

private:
  std::unordered_map<std::string, ValuePtr> _storage;
};
