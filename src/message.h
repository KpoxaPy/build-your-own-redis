#pragma once

#include <iostream>
#include <optional>
#include <string>
#include <variant>
#include <vector>

class Message {
public:
  enum class Type {
    Undefined,
    SimpleString,
    SimpleError,
    Integer,
    BulkString,
    Array,
  };

  using ValueType = std::variant<std::string, int, std::vector<Message>>;

  Message(Type type, ValueType value = {});

  Type type() const;
  void setValue(ValueType&& value);
  const ValueType& getValue() const;

  friend std::ostream& operator<<(std::ostream&, const Message&);
  std::string to_string() const;

private:
  Type _type;
  ValueType _value;
};
