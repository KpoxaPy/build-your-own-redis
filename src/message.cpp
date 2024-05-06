#include "message.h"

#include "message_common.h"

#include <sstream>

Message::Message(Message::Type type, ValueType value)
  : _type(type)
  , _value(std::move(value))
{
}

Message::Type Message::type() const {
  return this->_type;
}

void Message::setValue(ValueType&& value) {
  this->_value = value;
}

const Message::ValueType& Message::getValue() const {
  return this->_value;
}

std::string Message::to_string() const {
  std::ostringstream ss;
  if (this->_type == Message::Type::Undefined) {
    ss << MESSAGE_UNDEFINED << "\r\n";
  } else if (this->_type == Message::Type::SimpleString) {
    ss << MESSAGE_SIMPLE_STRING << std::get<std::string>(this->_value) << "\r\n";
  } else if (this->_type == Message::Type::SimpleError) {
    ss << MESSAGE_SIMPLE_ERROR << std::get<std::string>(this->_value) << "\r\n";
  } else if (this->_type == Message::Type::Integer) {
    ss << MESSAGE_INTEGER << std::get<int>(this->_value) << "\r\n";
  } else if (this->_type == Message::Type::BulkString) {
    const auto& data = std::get<std::string>(this->_value);
    if (data.size() == 0) {
      ss << MESSAGE_BULK_STRING << "-1\r\n";
    } else {
      ss << MESSAGE_BULK_STRING << data.size() << "\r\n";
      ss << data << "\r\n";
    }
  } else if (this->_type == Message::Type::SyncResponse) {
    const auto& data = std::get<std::string>(this->_value);
    ss << MESSAGE_BULK_STRING << data.size() << "\r\n" << data;
  } else if (this->_type == Message::Type::Array) {
    const auto& elements = std::get<std::vector<Message>>(this->_value);
    if (elements.size() == 0) {
      ss << MESSAGE_ARRAY << "-1\r\n";
    } else {
      ss << MESSAGE_ARRAY << elements.size() << "\r\n";
      for (const auto& element : elements) {
        ss << element.to_string();
      }
    }
  }
  return ss.str();
}

std::ostream& operator<<(std::ostream& stream, const Message& message) {
  if (message._type == Message::Type::Undefined) {
    stream << MESSAGE_UNDEFINED << std::endl;
  } else if (message._type == Message::Type::SimpleString) {
    stream << MESSAGE_SIMPLE_STRING << std::get<std::string>(message._value) << std::endl;
  } else if (message._type == Message::Type::SimpleError) {
    stream << MESSAGE_SIMPLE_ERROR << std::get<std::string>(message._value) << std::endl;
  } else if (message._type == Message::Type::Integer) {
    stream << MESSAGE_INTEGER << std::get<int>(message._value) << std::endl;
  } else if (message._type == Message::Type::BulkString) {
    const auto& data = std::get<std::string>(message._value);
    if (data.size() == 0) {
      stream << MESSAGE_BULK_STRING << "-1" << std::endl;
    } else {
      stream << MESSAGE_BULK_STRING << data.size() << std::endl;
      stream << data << std::endl;
    }
  } else if (message._type == Message::Type::SyncResponse) {
    const auto& data = std::get<std::string>(message._value);
    stream << MESSAGE_BULK_STRING << data.size() << std::endl;
    stream << "[file contents, size = " << data.size() << "]" << std::endl;
  } else if (message._type == Message::Type::Array) {
    const auto& elements = std::get<std::vector<Message>>(message._value);
    if (elements.size() == 0) {
      stream << MESSAGE_ARRAY << "-1" << std::endl;
    } else {
      stream << MESSAGE_ARRAY << elements.size() << std::endl;
      for (const auto& element : elements) {
        stream << element;
      }
    }
  }
  return stream;
}
