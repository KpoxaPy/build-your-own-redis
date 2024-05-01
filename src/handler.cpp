#include "handler.h"

#include "command.h"
#include "message.h"
#include "server.h"
#include "utils.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

class ConnReset : std::runtime_error {
public:
  ConnReset() : std::runtime_error("Conn reset") {}
};

Handler::Handler(int fd, Server& server, Storage& storage)
  : _client_fd(fd)
  , _begin_time(std::chrono::steady_clock::now())
  , _server(server)
  , _storage(storage)
{
}

Handler::~Handler() {
  close();
}

Server& Handler::server() {
  return this->_server;
}

Storage& Handler::storage() {
  return this->_storage;
}

Handler::Handler(Handler&& other)
  : _storage(other._storage)
  , _server(other._server)
{
  this->_client_fd = std::move(other._client_fd);
  other._client_fd.reset();

  this->_begin_time = std::move(other._begin_time);
  this->_buffer = std::move(other._buffer);
  this->_raw_messages = std::move(other._raw_messages);
}

Handler::ProcessStatus Handler::process() {
  try {
    this->read();

    while (auto maybe_message = Message::ParseFrom(this->_raw_messages)) {
      const auto& message = maybe_message.value();

      std::cerr << "<< FROM" << std::endl << message;

      try {
        auto command = Command::try_parse(message);
        command->reply(*this);
      } catch (const CommandParseError& err) {
        this->send(Message(Message::Type::SimpleError, {err.what()}));
      }
    }
  } catch (const ConnReset&) {
    return ProcessStatus::Closed;
  }

  return ProcessStatus::Keep;
}

void Handler::close() {
  if (this->_client_fd) {
    ::close(this->_client_fd.value());
    this->_client_fd.reset();
  }
}

std::size_t Handler::read() {
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

std::size_t Handler::parse_raw_messages() {
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

void Handler::send(const Message& message) {
  auto str = message.to_string();
  std::cerr << ">> TO" << std::endl;
  std::cerr << str;
  this->send(str);
}

void Handler::send(const std::string& str) {
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
