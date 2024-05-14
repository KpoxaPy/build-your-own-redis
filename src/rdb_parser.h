#pragma once

#include <chrono>
#include <exception>
#include <istream>
#include <optional>
#include <string>

using Clock = std::chrono::system_clock;
using Timepoint = Clock::time_point;

class RDBParseError : public std::runtime_error {
public:
  RDBParseError(std::string);
};

class IRDBParserListener {
public:
  virtual ~IRDBParserListener() = default;

  virtual void restore(std::string key, std::string value, std::optional<Timepoint> expire_time) = 0;
};

void RDBParse(std::istream& from, IRDBParserListener& to);