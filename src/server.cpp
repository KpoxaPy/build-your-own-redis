#include "server.h"

#include "utils.h"

#include <arpa/inet.h>
#include <cstdlib>
#include <cstring>
#include <netdb.h>
#include <sstream>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>


constexpr const int CONN_BACKLOG = 5;
constexpr const int DEFAULT_PORT = 6379;



ServerInfo ServerInfo::build(std::size_t argc, char** argv) {
  ServerInfo info;

  info.server.tcp_port = DEFAULT_PORT;
  info.replication.role = "master";
  info.replication.master_replid = random_hexstring(40);
  info.replication.master_repl_offset = 0;

  int arg_pos = 1;
  while (arg_pos < argc) {
    if (std::string("--port") == argv[arg_pos]) {
      if (arg_pos + 1 >= argc) {
        throw std::runtime_error("--port argument requires argument");
      }

      info.server.tcp_port = std::atoi(argv[arg_pos + 1]);
      arg_pos += 2;
    } else if (std::string("--replicaof") == argv[arg_pos]) {
      if (arg_pos + 2 >= argc) {
        throw std::runtime_error("--replicaof argument requires 2 arguments");
      }
      
      info.replication.role = "slave";
      info.replication.master_host = argv[arg_pos + 1];
      info.replication.master_port = std::atoi(argv[arg_pos + 2]);
      arg_pos += 3;
    } else {
      std::ostringstream ss;
      ss << "unexpected argument: " << argv[arg_pos];
      throw std::runtime_error(ss.str());
    }
  }

  return info;
}

std::string ServerInfo::to_string(std::unordered_set<std::string> parts) const {
  std::ostringstream ss;

  if (parts.contains("server")) {
    ss << this->server.to_string();
  }

  if (parts.contains("replication")) {
    ss << this->replication.to_string();
  }

  return ss.str();
}

std::string ServerInfo::Server::to_string() const {
  std::ostringstream ss;

  ss << "#Server" << std::endl;
  ss << "tcp_port:" << this->tcp_port << std::endl;

  return ss.str();
}

std::string ServerInfo::Replication::to_string() const {
  std::ostringstream ss;

  ss << "#Replication" << std::endl;
  ss << "role:" << this->role << std::endl;
  ss << "master_replid:" << this->master_replid << std::endl;
  ss << "master_repl_offset:" << this->master_repl_offset << std::endl;

  if (this->role == "slave") {
    ss << "master_host:" << this->master_host << std::endl;
    ss << "master_port:" << this->master_port << std::endl;
  }

  return ss.str();
}

Server::Server(Storage& storage, ServerInfo info)
  : _storage(storage)
  , _info(std::move(info))
{
}

Server::~Server() {
  this->close();
}

void Server::start(EventLoopManagerPtr event_loop) {
  this->_event_loop = event_loop;

  int server_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
  if (server_fd < 0) {
    throw std::runtime_error("Failed to create server socket");
  }

  this->_server_fd = server_fd;

  int reuse = 1;
  if (setsockopt(*this->_server_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) {
    throw std::runtime_error("Setsockopt failed");
  }

  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(this->_info.server.tcp_port);

  if (bind(*this->_server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
    std::ostringstream ss;
    ss << "Failed to bind to port " << this->_info.server.tcp_port;
    throw std::runtime_error(ss.str());
  }

  if (listen(*this->_server_fd, CONN_BACKLOG) != 0) {
    throw std::runtime_error("Listen failed");
  }

  auto accept_descriptor = _event_loop->make_desciptor();
  this->_event_loop->listen(accept_descriptor, [this](const Event& event) {
    auto& poll_event = static_cast<const PollEvent&>(event);

    if (poll_event.type != PollEventType::ReadyToRead) {
      throw std::runtime_error("Server got unexpected poll event");
      // TODO make error more verbose
    }

    try {
      while (auto maybe_client = this->accept()) {
        this->_handlers.push_back(std::move(maybe_client.value()));
      }
    } catch (std::exception& e) {
      std::cerr << "Client accepting error: " << e.what() << std::endl;
    }
  });
  this->_event_loop->post(PollAddEvent::make(*this->_server_fd, {PollEventType::ReadyToRead}, accept_descriptor));

  this->_event_loop->repeat([this]() {
    std::list<std::list<Handler>::iterator> handler_to_cleanup;
    for (auto handler_it = this->_handlers.begin(); handler_it != this->_handlers.end(); ++handler_it) {
      try {
        if (handler_it->process() == Handler::ProcessStatus::Closed) {
          handler_to_cleanup.push_back(handler_it);
        }
      } catch (std::exception& e) {
        std::cerr << "Client processing error: " << e.what() << std::endl;
      }
    }

    for (auto& handler_it: handler_to_cleanup) {
      this->_handlers.erase(handler_it);
    }
  });
}

std::optional<Handler> Server::accept() {
  struct sockaddr_in client_addr;
  std::size_t client_addr_len = sizeof(client_addr);

  int accept_res = ::accept(*this->_server_fd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_len);
  if (accept_res < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return {};
    } else {
      std::ostringstream ss;
      ss << "Server got error on accept: errno=" << errno;
      throw std::runtime_error(ss.str());
    }
  }

  return std::move(Handler(accept_res, *this, this->_storage));
}

ServerInfo& Server::info() {
  return this->_info;
}

void Server::close() {
  if (this->_server_fd) {
    ::close(this->_server_fd.value());
    this->_server_fd.reset();
  }
}
