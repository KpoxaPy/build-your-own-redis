#pragma once

#include "events.h"
#include "command.h"

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

using Clock = std::chrono::steady_clock;
using Timepoint = Clock::time_point;

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

class Storage {
public:
  Storage(EventLoopPtr event_loop);

  void start();

  void set(std::string key, std::string value, std::optional<int> expire_ms);
  std::optional<std::string> get(std::string key);

private:
  EventLoopPtr _event_loop;

  std::unordered_map<std::string, Value> _storage;
};
using StoragePtr = std::shared_ptr<Storage>;