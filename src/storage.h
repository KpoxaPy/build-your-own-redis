#pragma once

#include "events.h"

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

using Clock = std::chrono::steady_clock;
using Timepoint = Clock::time_point;

class IStorage {
public:
  virtual ~IStorage() = default;

  virtual void set(std::string key, std::string value, std::optional<int> expire_ms) = 0;
  virtual std::optional<std::string> get(std::string key) = 0;

  virtual std::vector<std::string> keys(std::string_view selector) const = 0;

};
using IStoragePtr = std::shared_ptr<IStorage>;

class Value {
public:
  Value(std::string data = "");

  const std::string& data() const;

  void setExpire(std::chrono::milliseconds ms);
  std::optional<Timepoint> getExpire() const;

private:
  std::string _data;
  Timepoint _create_time;
  std::optional<Timepoint> _expire_time;
};

class Storage : public IStorage {
public:
  Storage(EventLoopPtr event_loop);

  void set(std::string key, std::string value, std::optional<int> expire_ms) override;
  std::optional<std::string> get(std::string key) override;

  std::vector<std::string> keys(std::string_view selector) const override;

private:
  EventLoopPtr _event_loop;

  std::unordered_map<std::string, Value> _storage;
};