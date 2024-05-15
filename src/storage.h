#pragma once

#include "rdb_parser.h"

#include <chrono>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

enum class StorageType {
  None,
  String,
};

std::string to_string(StorageType type);

class IStorage : public IRDBParserListener {
public:
  virtual ~IStorage() = default;

  virtual void set(std::string key, std::string value, std::optional<int> expire_ms) = 0;
  virtual std::optional<std::string> get(std::string key) = 0;
  virtual StorageType type(std::string key) = 0;

  virtual std::vector<std::string> keys(std::string_view selector) const = 0;

};
using IStoragePtr = std::shared_ptr<IStorage>;

class Value {
public:
  Value(std::string data = "");

  const std::string& data() const;

  void setExpire(std::chrono::milliseconds ms);
  void setExpireTime(Timepoint tp);
  std::optional<Timepoint> getExpire() const;

private:
  std::string _data;
  Timepoint _create_time;
  std::optional<Timepoint> _expire_time;
};

class Storage : public IStorage {
public:
  Storage();

  void restore(std::string key, std::string value, std::optional<Timepoint> expire_time) override;

  void set(std::string key, std::string value, std::optional<int> expire_ms) override;
  std::optional<std::string> get(std::string key) override;
  StorageType type(std::string key) override;

  std::vector<std::string> keys(std::string_view selector) const override;

private:
  std::unordered_map<std::string, Value> _storage;
};