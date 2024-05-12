#include "handler.h"

#include "command.h"
#include "debug.h"
#include "handlers_manager.h"
#include "message.h"
#include "poller.h"
#include "server.h"
#include "utils.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

class ConnReset : std::runtime_error {
public:
  ConnReset() : std::runtime_error("Conn reset") {}
};

Handler::Handler(EventLoopPtr event_loop, int fd, TalkerPtr talker)
  : _event_loop(event_loop)
  , _fd(fd)
  , _talker(talker)
  , _parser(this->_read_buffer)
{
  this->_slot_fd_event = std::make_shared<Slot<PollEventType>>([this](PollEventType type) {
    if (!this->_fd) {
      return;
    }

    if (type == PollEventType::ReadyToRead) {
      this->process_read();
    } else if (type == PollEventType::ReadyToWrite) {
      this->write();
    } else if (type == PollEventType::HangUp) {
      this->close();
    } else {
      this->close();
      throw std::runtime_error("Handler got unexpected poll event");
      // TODO make error more verbose
    }
  });

  this->_new_fd_signal = std::make_shared<Signal<int, PollEventTypeList, SignalPtr<PollEventType>>>();
  this->_removed_fd_signal = std::make_shared<Signal<int>>();
  this->_fd_event_signal = std::make_shared<Signal<PollEventType>>();
  this->_fd_event_signal->connect(this->_slot_fd_event);

  this->_event_loop->post([this](){
    this->start();
  });
}

Handler::~Handler() {
  this->close();
}

SignalPtr<int, PollEventTypeList, SignalPtr<PollEventType>>& Handler::new_fd() {
  return this->_new_fd_signal;
}

SignalPtr<int>& Handler::removed_fd() {
  return this->_removed_fd_signal;
}

void Handler::start() {
  if (!this->_fd) {
    return;
  }

  fcntl(this->_fd.value(), F_SETFL, O_NONBLOCK);

  this->setup_poll(false);

  this->_event_loop->repeat([this]() {
    this->process_write();
  });
}

void Handler::setup_poll(bool write) {
  if (write) {
    this->_new_fd_signal->emit(
        this->_fd.value(),
        PollEventTypeList{PollEventType::ReadyToRead, PollEventType::ReadyToWrite},
        this->_fd_event_signal);
  } else {
    this->_new_fd_signal->emit(
        this->_fd.value(),
        PollEventTypeList{PollEventType::ReadyToRead},
        this->_fd_event_signal);
  }
}

void Handler::close() {
  if (this->_fd) {
    auto fd = this->_fd.value();
    this->_fd.reset();

    if (this->_talker) {
      this->_talker->interrupt();
    }

    this->_removed_fd_signal->emit(fd);
    ::close(fd);
  }
}

void Handler::process_read() {
  try {
    this->read();

    while (auto maybe_message = this->_parser.try_parse(this->_talker->expected())) {
      if (DEBUG_LEVEL >= 1) std::cerr << "<< FROM" << std::endl << maybe_message.value();
      this->_talker->listen(std::move(maybe_message.value()));
    }
  } catch (const ConnReset&) {
    this->close();
  }

  this->process_write();
}

void Handler::process_write() {
  try {
    while (auto maybe_message = this->_talker->say()) {
      if (maybe_message.value().type() == Message::Type::Leave) {
        this->close();
        break;
      }
      this->send(maybe_message.value());
    }
  } catch (const ConnReset&) {
    this->close();
  }
}

void Handler::read() {
  static constexpr std::size_t READ_BUFFER_SIZE = 1024;
  std::array<char, READ_BUFFER_SIZE> read_buffer;

  while (true) {
    ssize_t read_size = ::read(this->_fd.value(), read_buffer.data(), READ_BUFFER_SIZE);

    if (DEBUG_LEVEL >= 2) std::cerr << "DEBUG read from fd=" << this->_fd.value()
      << " transferred=" << read_size << std::endl;

    if (read_size > 0) {
      this->_read_buffer.insert(this->_read_buffer.end(), read_buffer.begin(), read_buffer.begin() + read_size);
      continue;
    }

    if (read_size < 0) {
      if (DEBUG_LEVEL >= 2) std::cerr << "DEBUG read resulted in error: " << strerror(errno) << std::endl;
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      } else if (errno == ECONNRESET) {
        throw ConnReset();
      } else {
        std::ostringstream ss;
        ss << "Error recv from client(" << this->_fd.value() << "): errno=" << errno;
        throw std::runtime_error(ss.str());
      }
    }

    if (read_size == 0) {
      throw ConnReset();
    }

    break;
  }
}

void Handler::write() {
  static constexpr std::size_t WRITE_BUFFER_SIZE = 1024;
  std::array<char, WRITE_BUFFER_SIZE> write_buffer;

  if (this->_write_buffer.size() == 0) {
    return;
  }

  if (DEBUG_LEVEL >= 2) std::cerr << "DEBUG write_buffer size=" << this->_write_buffer.size() << std::endl;

  std::size_t transferred_total = 0;
  while (transferred_total < this->_write_buffer.size()) {
    const auto max_write = std::min(transferred_total + WRITE_BUFFER_SIZE, this->_write_buffer.size());
    const auto write_buffer_len_filled = max_write - transferred_total;

    copy(
      this->_write_buffer.begin() + transferred_total, 
      this->_write_buffer.begin() + max_write,
      write_buffer.begin());


    ssize_t transferred = ::write(
      this->_fd.value(),
      write_buffer.data(),
      write_buffer_len_filled
    );

    if (DEBUG_LEVEL >= 2) std::cerr << "DEBUG write to fd=" << this->_fd.value()
      << " transferred=" << transferred << std::endl;

    if (transferred < 0) {
      if (DEBUG_LEVEL >= 2) std::cerr << "DEBUG write resulted in error: " << strerror(errno) << std::endl;
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      } else if (errno == ECONNRESET) {
        throw ConnReset();
      } else {
        std::ostringstream ss;
        ss << "Error write to client(" << this->_fd.value() << "): errno=" << errno;
        throw std::runtime_error(ss.str());
      }
    } else {
      transferred_total += transferred;
    }
  }

  if (transferred_total > 0) {
    this->_write_buffer.erase(this->_write_buffer.begin(), this->_write_buffer.begin() + transferred_total);
  }

  if (this->_write_buffer.size() == 0) {
    this->setup_poll(false);
  } else {
    this->setup_poll(true);
  }
}

void Handler::send(const Message& message) {
  auto str = message.to_string();
  if (DEBUG_LEVEL >= 1) {
    std::cerr << ">> TO" << std::endl;
    std::cerr << message;
  }
  this->send(str);
}

void Handler::send(const std::string& str) {
  this->_write_buffer.insert(this->_write_buffer.end(), str.begin(), str.begin() + str.size());
  this->write();
}
