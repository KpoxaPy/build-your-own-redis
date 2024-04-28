#include "client.h"

#include "request.h"
#include "response.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

class ConnReset : std::runtime_error {
public:
  ConnReset() : std::runtime_error("Conn reset") {}
};

std::ostream& operator<<(std::ostream& stream, Client::RawMessage raw_message) {
  for (const auto& b: raw_message) {
    stream << static_cast<char>(b);
  }
  return stream;
}

Client::Client(int fd) {
  this->_client_fd = fd;
  this->_begin_time = std::chrono::steady_clock::now();
}

Client::~Client() {
  close();
}

Client::Client(Client&& other) {
  this->_client_fd = std::move(other._client_fd);
  other._client_fd.reset();

  this->_begin_time = std::move(other._begin_time);
  this->_buffer = std::move(other._buffer);
  this->_raw_messages = std::move(other._raw_messages);
}

Client::ProcessStatus Client::process() {
  try {
    auto new_messages = this->read();

    if (new_messages == 0) {
      if (this->met_new_message) {
        this->send("+PONG\r\n");
        this->met_new_message = false;
      }
    } else {
      this->met_new_message = true;
    }

    while (!this->_raw_messages.empty()) {
      std::cout << "< " << this->_raw_messages.front() << std::endl;
      this->_raw_messages.pop();
    }
  } catch (const ConnReset&) {
    return ProcessStatus::Closed;
  }

  return ProcessStatus::Keep;
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

    this->_raw_messages.emplace(this->_buffer.begin(), result_it);
    this->_buffer.erase(this->_buffer.begin(), result_it + delim.size());
    ++new_messages;
  }

  return new_messages;
}

void Client::send(const std::string& str) {
  std::size_t transferred_total = 0;
  std::cerr << "> " << str << std::endl;
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
