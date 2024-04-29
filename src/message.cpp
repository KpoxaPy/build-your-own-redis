#include "message.h"
#include "utils.h"

#include <queue>
#include <sstream>
#include <stdexcept>

class ParseHelper {
  RawMessagesStream& _stream;
  std::queue<RawMessage> _unget_queue;

public:
  ParseHelper(RawMessagesStream& stream)
    : _stream(stream)
  {
  }

  std::size_t size() const {
    return this->_stream.size();
  }

  RawMessage get() {
    auto message = this->_stream.front();
    this->_stream.pop_front();
    this->_unget_queue.push(message);
    return std::move(message);
  }

  void clear_unget() {
    this->_unget_queue = {};
  }

  void unget() {
    this->_stream.push_front(this->_unget_queue.front());
    this->_unget_queue.pop();
  }

  void unget_all() {
    while (this->_unget_queue.size() > 0) {
      this->unget();
    }
  }

  std::optional<Message> parse() {
    if (this->size() == 0) {
      return {};
    }

    auto raw_message = this->get();
    if (raw_message[0] == std::byte('+')) {
      auto message = Message(Message::Type::SimpleString);
      message.setValue(std::string{reinterpret_cast<const char*>(raw_message.data() + 1), raw_message.size() - 1});
      return message;
    } else if (raw_message[0] == std::byte('-')) {
      auto message = Message(Message::Type::SimpleError);
      message.setValue(std::string{reinterpret_cast<const char*>(raw_message.data() + 1), raw_message.size() - 1});
      return message;
    } else if (raw_message[0] == std::byte(':')) {
      auto message = Message(Message::Type::Integer);
      if (raw_message.size() > 1) {
        if (auto value = parseInt(reinterpret_cast<const char*>(raw_message.data() + 1), raw_message.size() - 1)) {
          message.setValue(value.value());
          return message;
        }
      }

      std::ostringstream ss;
      ss << "Malformed integer message: " << raw_message;
      throw std::runtime_error(ss.str());
    } else if (raw_message[0] == std::byte('$')) {
      Message message = Message(Message::Type::BulkString);
      if (raw_message.size() == 1) {
        std::ostringstream ss;
        ss << "Malformed length for bulk string message: " << raw_message;
        throw std::runtime_error(ss.str());
      }

      int length;
      if (auto value = parseInt(reinterpret_cast<const char*>(raw_message.data() + 1), raw_message.size() - 1)) {
        length = value.value();
      } else {
        std::ostringstream ss;
        ss << "Malformed length for bulk string message: " << raw_message;
        throw std::runtime_error(ss.str());
      }

      if (length == -1) {
        return message;
      }

      if (this->size() == 0) {
        this->unget_all();
        return {};
      }

      auto bulk_data = this->get();
      message.setValue(std::string{reinterpret_cast<const char*>(bulk_data.data()), bulk_data.size()});
      return message;
    } else if (raw_message[0] == std::byte('*')) {
      auto message = Message(Message::Type::Array);
      if (raw_message.size() == 1) {
        std::ostringstream ss;
        ss << "Malformed length for bulk string message: " << raw_message;
        throw std::runtime_error(ss.str());
      }

      int length;
      if (auto value = parseInt(reinterpret_cast<const char*>(raw_message.data() + 1), raw_message.size() - 1)) {
        length = value.value();
      } else {
        std::ostringstream ss;
        ss << "Malformed length for bulk string message: " << raw_message;
        throw std::runtime_error(ss.str());
      }

      if (length == -1) {
        message.setValue(std::vector<Message>{});
        return message;
      }

      std::vector<Message> messages;
      for (int i = 0; i < length; ++i) {
        auto element = this->parse();
        if (!element) {
          this->unget_all();
          return {};
        }
        messages.emplace_back(std::move(element.value()));
      }

      message.setValue(messages);
      return message;
    } else {
      std::ostringstream ss;
      ss << "Unknown message type: " << raw_message;
      throw std::runtime_error(ss.str());
    }

    throw std::runtime_error("Broken parser");
  }
};

std::optional<Message> Message::ParseFrom(RawMessagesStream& stream) {
  ParseHelper helper(stream);
  return helper.parse();
}

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
    ss << "U" << "\r\n";
  } else if (this->_type == Message::Type::SimpleString) {
    ss << "+" << std::get<std::string>(this->_value) << "\r\n";
  } else if (this->_type == Message::Type::SimpleError) {
    ss << "-" << std::get<std::string>(this->_value) << "\r\n";
  } else if (this->_type == Message::Type::Integer) {
    ss << ":" << std::get<int>(this->_value) << "\r\n";
  } else if (this->_type == Message::Type::BulkString) {
    const auto& data = std::get<std::string>(this->_value);
    if (data.size() == 0) {
      ss << "$-1\r\n";
    } else {
      ss << "$" << data.size() << "\r\n";
      ss << data << "\r\n";
    }
  } else if (this->_type == Message::Type::Array) {
    const auto& elements = std::get<std::vector<Message>>(this->_value);
    if (elements.size() == 0) {
      ss << "*-1\r\n";
    } else {
      ss << "*" << elements.size() << "\r\n";
      for (const auto& element : elements) {
        ss << element;
      }
    }
  }
  return ss.str();
}

std::ostream& operator<<(std::ostream& stream, const Message& message) {
  if (message._type == Message::Type::Undefined) {
    stream << "U" << std::endl;
  } else if (message._type == Message::Type::SimpleString) {
    stream << "+" << std::get<std::string>(message._value) << std::endl;
  } else if (message._type == Message::Type::SimpleError) {
    stream << "-" << std::get<std::string>(message._value) << std::endl;
  } else if (message._type == Message::Type::Integer) {
    stream << ":" << std::get<int>(message._value) << std::endl;
  } else if (message._type == Message::Type::BulkString) {
    const auto& data = std::get<std::string>(message._value);
    if (data.size() == 0) {
      stream << "$-1" << std::endl;
    } else {
      stream << "$" << data.size() << std::endl;
      stream << data << std::endl;
    }
  } else if (message._type == Message::Type::Array) {
    const auto& elements = std::get<std::vector<Message>>(message._value);
    if (elements.size() == 0) {
      stream << "*-1" << std::endl;
    } else {
      stream << "*" << elements.size() << std::endl;
      for (const auto& element : elements) {
        stream << element;
      }
    }
  }
  return stream;
}
