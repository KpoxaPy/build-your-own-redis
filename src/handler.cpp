#include "handler.h"

#include "command.h"
#include "handlers_manager.h"
#include "message.h"
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

Handler::Handler(int fd, HandlersManager& manager)
  : _fd(fd)
  , _manager(manager)
  , _parser(this->_read_buffer)
{
}

Handler::Handler(Handler&& other)
  : _manager(other._manager)
  , _event_loop(std::move(other._event_loop))
  , _read_buffer(std::move(other._read_buffer))
  , _write_buffer(std::move(other._write_buffer))
  , _parser(std::move(other._parser))
  , _handler_desciptor(std::move(other._handler_desciptor))
{
  this->_fd = std::move(other._fd);
  other._fd.reset();
}

Handler::~Handler() {
  close();
}

void Handler::start(EventLoopManagerPtr event_loop) {
  this->_event_loop = event_loop;

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
    this->_event_loop->post(PollAddEvent::make(
        this->_fd.value(),
        {PollEventType::ReadyToRead, PollEventType::ReadyToWrite},
        this->_handler_desciptor));
  } else {
    this->_event_loop->post(PollAddEvent::make(
        this->_fd.value(),
        {PollEventType::ReadyToRead},
        this->_handler_desciptor));
  }
}

void Handler::close() {
  if (this->_fd) {
    this->_event_loop->post(PollRemoveEvent::make(this->_fd.value()));
    this->_event_loop->post(HandlerRemoveEvent::make(this->_fd.value()));

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

      try {
        this->reply(Command::try_parse(message));
      } catch (const CommandParseError& err) {
        this->send(Message(Message::Type::SimpleError, {err.what()}));
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

void Handler::reply(CommandPtr command) {
  if (command->type() == CommandType::Ping) {
    this->send(Message(Message::Type::SimpleString, {"PONG"}));

  } else if (command->type() == CommandType::Echo) {
    auto echo_command = static_cast<EchoCommand&>(*command);

    this->send(Message(Message::Type::BulkString, echo_command.data()));

  } else if (command->type() == CommandType::Set) {
    auto set_command = static_cast<SetCommand&>(*command);

    this->_manager.storage()->storage[set_command.key()] = set_command.value();
    Value& stored_value = this->_manager.storage()->storage[set_command.key()];

    if (set_command.expire_ms()) {
      stored_value.setExpire(std::chrono::milliseconds{set_command.expire_ms().value()});
    }

    this->send(Message(Message::Type::SimpleString, "OK"));

  } else if (command->type() == CommandType::Get) {
    auto get_command = static_cast<GetCommand&>(*command);

    auto it = this->_manager.storage()->storage.find(get_command.key());
    if (it == this->_manager.storage()->storage.end()) {
      this->send(Message(Message::Type::BulkString, {}));
      return;
    }
    auto& value = it->second;

    if (value.getExpire() && Clock::now() >= value.getExpire()) {
      this->_manager.storage()->storage.erase(get_command.key());
      this->send(Message(Message::Type::BulkString, {}));
      return;
    }

    this->send(Message(Message::Type::BulkString, value.data()));

  } else if (command->type() == CommandType::Info) {
    auto info_command = static_cast<InfoCommand&>(*command);

    std::unordered_set<std::string> info_parts;

    auto default_parts = [&info_parts]() {
      info_parts.insert("server");
      info_parts.insert("replication");
    };

    for (const auto& info_part : info_command.args()) {
      if (info_part == "default") {
        default_parts();
      } else {
        info_parts.insert(info_part);
      }
    }

    if (info_parts.size() == 0) {
      default_parts();
    }

    this->send(Message(Message::Type::BulkString, this->_manager.server()->info().to_string(info_parts)));

  } else {
    this->send(Message(Message::Type::SimpleError, {"unimplemented command"}));
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
