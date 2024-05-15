#include "message_parser.h"

#include "message_common.h"
#include "utils.h"

#include <algorithm>
#include <stack>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

const std::string DELIM = "\r\n";
const std::unordered_set<char> EXPECTED_TYPES = {
  '+', '-', ':', '$', '*'
};
const std::unordered_set<char> LENGTH_TYPES = {
  '$'
};


template class MessageParser<std::deque<char>>;

template<typename T>
class ParseHelper {
public:
  using RawMessage = typename MessageParser<T>::RawMessage;
  using RawMessageBuffer = typename MessageParser<T>::RawMessageBuffer;

  RawMessageBuffer& buffer;
  std::stack<RawMessage> unget_stack;

  ParseHelper(RawMessageBuffer& buffer)
    : buffer(buffer) {
  }

  std::size_t size() const {
    return this->buffer.size();
  }

  RawMessage get() {
    auto message = this->buffer.front();
    this->buffer.pop_front();
    this->unget_stack.push(message);
    return std::move(message);
  }

  void clear_unget() {
    this->unget_stack = {};
  }

  void unget() {
    this->buffer.push_front(std::move(this->unget_stack.top()));
    this->unget_stack.pop();
  }

  void unget_all() {
    while (this->unget_stack.size() > 0) {
      this->unget();
    }
  }

  std::optional<Message> parse(Message::Type expected = Message::Type::Any) {
    if (this->size() == 0) {
      return {};
    }

    auto raw_message = this->get();
    if (raw_message[0] == MESSAGE_SIMPLE_STRING) {
      auto message = Message(Message::Type::SimpleString);
      message.setValue(std::string{raw_message.begin() + 1, raw_message.end()});
      return message;
    } else if (raw_message[0] == MESSAGE_SIMPLE_ERROR) {
      auto message = Message(Message::Type::SimpleError);
      message.setValue(std::string{raw_message.begin() + 1, raw_message.end()});
      return message;
    } else if (raw_message[0] == MESSAGE_INTEGER) {
      auto message = Message(Message::Type::Integer);
      if (raw_message.size() > 1) {
        if (auto value = parseInt({raw_message.begin() + 1, raw_message.end()})) {
          message.setValue(value.value());
          return message;
        }
      }

      std::ostringstream ss;
      ss << "Malformed integer message: " << raw_message;
      throw std::runtime_error(ss.str());
    } else if (raw_message[0] == MESSAGE_BULK_STRING) {
      Message message = Message(Message::Type::BulkString);
      if (expected == Message::Type::SyncResponse) {
        message = Message(Message::Type::SyncResponse);
      }

      if (raw_message.size() == 1) {
        std::ostringstream ss;
        ss << "Malformed length for bulk string message: " << raw_message;
        throw std::runtime_error(ss.str());
      }

      int length;
      if (auto value = parseInt({raw_message.begin() + 1, raw_message.end()})) {
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

      message.setValue(this->get());
      return message;
    } else if (raw_message[0] == MESSAGE_ARRAY) {
      auto message = Message(Message::Type::Array);
      if (raw_message.size() == 1) {
        std::ostringstream ss;
        ss << "Malformed length for array message: " << raw_message;
        throw std::runtime_error(ss.str());
      }

      int length;
      if (auto value = parseInt({raw_message.begin() + 1, raw_message.end()})) {
        length = value.value();
      } else {
        std::ostringstream ss;
        ss << "Malformed length for array message: " << raw_message;
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

template <typename T>
MessageParser<T>::MessageParser(T& buffer)
  : _buffer(buffer) {
}

template <typename T>
std::optional<Message> MessageParser<T>::try_parse(Message::Type expected) {
  ParseHelper<T> helper(this->_raw_message_buffer);

  while (this->_buffer.size() > 0) {
    if (!this->get_next_raw_message(expected)) {
      break;
    }

    if(auto message = helper.parse(expected)) {
      return std::move(message);
    }
  }
  
  return {};
}

template <typename T>
bool MessageParser<T>::get_next_raw_message(Message::Type expected) {
  if (this->_length_encoded_message_expected) {
    auto length = this->_length_encoded_message_expected.value();

    std::size_t delim_size = DELIM.size();
    if (expected == Message::Type::SyncResponse) {
      delim_size = 0;
    }

    if (this->_buffer.size() < length + delim_size) {
      return false;
    }

    auto first = this->_buffer.begin();
    auto last = this->_buffer.begin() + length;
    this->_raw_message_buffer.emplace_back(first, last);
    this->_buffer.erase(first, last + delim_size);

    this->_length_encoded_message_expected.reset();
    return true;
  }
  
  char type = this->_buffer[0];
  if (!EXPECTED_TYPES.contains(type)) {
    std::ostringstream ss;
    ss << "Unknown message type: " << type;
    throw std::runtime_error(ss.str());
  }

  auto result_it = std::search(this->_buffer.begin(), this->_buffer.end(), DELIM.begin(), DELIM.end());
  if (result_it == this->_buffer.end()) {
    return false;
  }

  this->_raw_message_buffer.emplace_back(this->_buffer.begin(), result_it);
  this->_buffer.erase(this->_buffer.begin(), result_it + DELIM.size());

  if (!LENGTH_TYPES.contains(type)) {
    return true;
  }

  auto& last = this->_raw_message_buffer.back();
  if (auto value = parseInt(last.data() + 1, last.size() - 1)) {
    this->_length_encoded_message_expected = value.value();
  } else {
    std::ostringstream ss;
    ss << "Malformed length for array message: " << last;
    throw std::runtime_error(ss.str());
  }

  return true;
}
