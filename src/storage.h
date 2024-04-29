#pragma once

#include <chrono>
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
  std::unordered_map<std::string, Value> storage;
};