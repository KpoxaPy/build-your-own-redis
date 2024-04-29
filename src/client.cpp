#include "client.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

#include "message.h"

class ConnReset : std::runtime_error {
public:
  ConnReset() : std::runtime_error("Conn reset") {}
};

Client::Client(int fd, Storage& storage)
  : _client_fd(fd)
  , _begin_time(std::chrono::steady_clock::now())
  , _storage(storage)
{
}

Client::~Client() {
  close();
}

Client::Client(Client&& other)
  : _storage(other._storage)
{
  this->_client_fd = std::move(other._client_fd);
  other._client_fd.reset();

  this->_begin_time = std::move(other._begin_time);
  this->_buffer = std::move(other._buffer);
  this->_raw_messages = std::move(other._raw_messages);
}

Client::ProcessStatus Client::process() {
  try {
    this->read();

    while (auto maybe_message = Message::ParseFrom(this->_raw_messages)) {
      const auto& message = maybe_message.value();

      std::cerr << "<< FROM" << std::endl << message;
      this->reply_to(message);
    }
  } catch (const ConnReset&) {
    return ProcessStatus::Closed;
  }

  return ProcessStatus::Keep;
}

void Client::reply_to(const Message& message) {
  if (message.type() != Message::Type::Array) {
    this->reply_unknown();
    return;
  }

  const auto& data = std::get<std::vector<Message>>(message.getValue());
  if (data.size() == 0) {
    this->reply_unknown();
    return;
  }

  if (data[0].type() != Message::Type::BulkString) {
    this->reply_unknown();
    return;
  }

  const std::string& messageStr = std::get<std::string>(data[0].getValue());
  std::string command = messageStr;
  std::transform(messageStr.begin(), messageStr.end(), command.begin(), [](unsigned char ch) { return std::tolower(ch); });

  if (command == "echo") {
    this->reply_to_echo(message);
  } else if (command == "ping") {
    this->reply_to_ping();
  } else if (command == "set") {
    this->reply_to_set(message);
  } else if (command == "get") {
    this->reply_to_get(message);
  } else {
    this->reply_unknown();
  }
}

void Client::reply_to_echo(const Message& message) {
  const auto& data = std::get<std::vector<Message>>(message.getValue());
  if (data.size() != 2) {
    std::ostringstream ss;
    ss << "ECHO command must have 1 argument, recieved " << data.size();
    this->send(Message(Message::Type::SimpleError, {ss.str()}));
    return;
  }

  if (data[1].type() != Message::Type::BulkString) {
    std::ostringstream ss;
    ss << "ECHO command must have first argument with type BulkString";
    this->send(Message(Message::Type::SimpleError, {ss.str()}));
    return;
  }

  const auto& msg = std::get<std::string>(data[1].getValue());
  this->send(Message(Message::Type::BulkString, {msg}));
}

void Client::reply_to_ping() {
  this->send(Message(Message::Type::SimpleString, {"PONG"}));
}

void Client::reply_to_set(const Message& message) {
  const auto& data = std::get<std::vector<Message>>(message.getValue());
  if (data.size() < 3) {
    std::ostringstream ss;
    ss << "SET command must have at least 2 arguments, recieved " << data.size();
    this->send(Message(Message::Type::SimpleError, {ss.str()}));
    return;
  }

  if (data[1].type() != Message::Type::BulkString) {
    std::ostringstream ss;
    ss << "SET command must have first argument with type BulkString";
    this->send(Message(Message::Type::SimpleError, {ss.str()}));
    return;
  }

  if (data[2].type() != Message::Type::BulkString) {
    std::ostringstream ss;
    ss << "SET command must have second argument with type BulkString";
    this->send(Message(Message::Type::SimpleError, {ss.str()}));
    return;
  }

  const auto& key = std::get<std::string>(data[1].getValue());
  const auto& value = std::get<std::string>(data[2].getValue());

  this->_storage.set(key, value);
  this->send(Message(Message::Type::SimpleString, "OK"));
}

void Client::reply_to_get(const Message& message) {
  const auto& data = std::get<std::vector<Message>>(message.getValue());
  if (data.size() < 2) {
    std::ostringstream ss;
    ss << "GET command must have at least 1 argument, recieved " << data.size();
    this->send(Message(Message::Type::SimpleError, {ss.str()}));
    return;
  }

  if (data[1].type() != Message::Type::BulkString) {
    std::ostringstream ss;
    ss << "GET command must have first argument with type BulkString";
    this->send(Message(Message::Type::SimpleError, {ss.str()}));
    return;
  }

  const auto& key = std::get<std::string>(data[1].getValue());

  if (auto maybe_value = this->_storage.get(key)) {
    this->send(Message(Message::Type::BulkString, maybe_value.value()));
    return;
  }

  this->send(Message(Message::Type::BulkString, {}));
}

void Client::reply_unknown() {
  this->send(Message(Message::Type::SimpleError, {"Unknown command"}));
}

void Client::close() {
  if (this->_client_fd) {
    ::close(this->_client_fd.value());
    this->_client_fd.reset();
  }
}

std::size_t Client::read() {
  static constexpr std::size_t READ_BUFFER_SIZE = 1024;
  std::array<std::byte, READ_BUFFER_SIZE> read_buffer;

  while (true) {
    ssize_t recv_size = recv(this->_client_fd.value(), read_buffer.data(), 1024, MSG_DONTWAIT);
    if (recv_size > 0) {
      this->_buffer.insert(this->_buffer.end(), read_buffer.begin(), read_buffer.begin() + recv_size);
      continue;
    }
    if (recv_size < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      } else if (errno == ECONNRESET) {
        throw ConnReset();
      } else {
        std::ostringstream ss;
        ss << "Error recv from client(" << this->_client_fd.value() << "): errno=" << errno;
        throw std::runtime_error(ss.str());
      }
    }
    break;
  }

  return this->parse_raw_messages();
}

std::size_t Client::parse_raw_messages() {
  size_t new_messages = 0;

  const RawMessage delim = {std::byte{'\r'}, std::byte{'\n'}};
  while (true) {
    auto result_it = std::search(this->_buffer.begin(), this->_buffer.end(), delim.begin(), delim.end());
    if (result_it == this->_buffer.end()) {
      break;
    }

    this->_raw_messages.emplace_back(this->_buffer.begin(), result_it);
    this->_buffer.erase(this->_buffer.begin(), result_it + delim.size());
    ++new_messages;
  }

  return new_messages;
}

void Client::send(const Message& message) {
  auto str = message.to_string();
  std::cerr << ">> TO" << std::endl;
  std::cerr << str;
  this->send(str);
}

void Client::send(const std::string& str) {
  std::size_t transferred_total = 0;
  while (transferred_total < str.size()) {
    ssize_t transferred = write(
      this->_client_fd.value(),
      str.c_str() + transferred_total,
      str.size() - transferred_total
    );
    if (transferred < 0) {
      if (errno == ECONNRESET) {
        throw ConnReset();
      } else {
        std::ostringstream ss;
        ss << "Error write to client(" << this->_client_fd.value() << "): errno=" << errno;
        throw std::runtime_error(ss.str());
      }
    } else {
      transferred_total += transferred;
    }
  }
}
