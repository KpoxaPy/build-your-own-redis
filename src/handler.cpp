#include "handler.h"

#include "command.h"
#include "handlers_manager.h"
#include "message.h"
#include "poller.h"
#include "server.h"
#include "utils.h"

#include <algorithm>
#include <array>
#include <cctype>
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
}

Handler::~Handler() {
  this->close();
}

void Handler::connect_poller_add(EventDescriptor descriptor) {
  this->_poller_add = descriptor;
}

void Handler::connect_poller_remove(EventDescriptor descriptor) {
  this->_poller_remove = descriptor;
}

void Handler::connect_handlers_manager_remove(EventDescriptor descriptor) {
  this->_handlers_manager_remove = descriptor;
}

void Handler::start() {
  if (!this->_fd) {
    return;
  }

  fcntl(this->_fd.value(), F_SETFL, O_NONBLOCK);

  this->_handler_desciptor = _event_loop->make_desciptor();
  this->_event_loop->listen(this->_handler_desciptor, [this](const Event& event) {
    auto& poll_event = static_cast<const PollEvent&>(event);

    if (poll_event.type == PollEventType::ReadyToRead) {
      this->process();
    } else if (poll_event.type == PollEventType::ReadyToWrite) {
      this->write();
    } else if (poll_event.type == PollEventType::HangUp) {
      this->close();
    } else {
      this->close();
      throw std::runtime_error("Server got unexpected poll event");
      // TODO make error more verbose
    }
  });
  this->setup_poll(false);
}

void Handler::setup_poll(bool write) {
  if (write) {
    this->_event_loop->post<PollAddEvent>(
        this->_poller_add,
        this->_fd.value(),
        PollEventTypeList{PollEventType::ReadyToRead, PollEventType::ReadyToWrite},
        this->_handler_desciptor);
  } else {
    this->_event_loop->post<PollAddEvent>(
        this->_poller_add,
        this->_fd.value(),
        PollEventTypeList{PollEventType::ReadyToRead},
        this->_handler_desciptor);
  }
}

void Handler::close() {
  std::cerr << "DEBUG Closing fd=" << this->_fd.value() << std::endl;
  if (this->_fd) {
    std::cerr << "DEBUG Closed fd=" << this->_fd.value() << std::endl;
    this->_event_loop->post<PollRemoveEvent>(this->_poller_remove, this->_fd.value());
    this->_event_loop->post<HandlerRemoveEvent>(this->_handlers_manager_remove, this->_fd.value());

    ::close(this->_fd.value());
    this->_fd.reset();
  }
}

void Handler::process() {
  try {
    this->read();

    while (auto maybe_message = this->_parser.try_parse()) {
      const auto& message = maybe_message.value();

      std::cerr << "<< FROM" << std::endl << message;

      if (auto reply = this->_talker->talk(message)) {
        this->send(reply.value());
      } else {
        this->close();
      }
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
    if (read_size > 0) {
      this->_read_buffer.insert(this->_read_buffer.end(), read_buffer.begin(), read_buffer.begin() + read_size);
      continue;
    }
    if (read_size < 0) {
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
    break;
  }
}

void Handler::write() {
  static constexpr std::size_t WRITE_BUFFER_SIZE = 1024;
  std::array<char, WRITE_BUFFER_SIZE> write_buffer;

  if (this->_write_buffer.size() == 0) {
    return;
  }

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

    if (transferred < 0) {
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
  std::cerr << ">> TO" << std::endl;
  std::cerr << str;
  this->send(str);
}

void Handler::send(const std::string& str) {
  this->_write_buffer.insert(this->_write_buffer.end(), str.begin(), str.begin() + str.size());
  this->write();
}
