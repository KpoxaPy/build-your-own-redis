#include "server.h"

#include "debug.h"
#include "handlers_manager.h"
#include "poller.h"
#include "utils.h"

#include <arpa/inet.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
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
        throw std::runtime_error("--port requires argument");
      }

      info.server.tcp_port = std::atoi(argv[arg_pos + 1]);
      arg_pos += 2;

    } else if (std::string("--replicaof") == argv[arg_pos]) {
      if (arg_pos + 1 >= argc) {
        throw std::runtime_error("--replicaof requires argument \"[host port]\"");
      }
      
      info.replication.role = "slave";
      std::string arg = argv[arg_pos + 1];
      auto delim_pos = arg.find(' ');

      if (delim_pos == arg.npos) {
        throw std::runtime_error("--replicaof requires argument [host port]");
      }
      
      info.replication.master_host = {arg.begin() , arg.begin() + delim_pos};
      info.replication.master_port = std::atoi(std::string(arg.begin() + delim_pos + 1, arg.end()).data());
      arg_pos += 2;

    } else if (std::string("--dir") == argv[arg_pos]) {
      if (arg_pos + 1 >= argc) {
        throw std::runtime_error("--dir requires argument");
      }

      info.server.dir = argv[arg_pos + 1];
      arg_pos += 2;

    } else if (std::string("--dbfilename") == argv[arg_pos]) {
      if (arg_pos + 1 >= argc) {
        throw std::runtime_error("--dbfilename requires argument");
      }

      info.server.dbfilename = argv[arg_pos + 1];
      arg_pos += 2;

    } else if (std::string("-v") == argv[arg_pos]) {
      info.debug_level = 1;

      arg_pos += 1;

    } else if (std::string("-vv") == argv[arg_pos]) {
      info.debug_level = 2;

      arg_pos += 1;

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

std::optional<std::string> ServerInfo::get_config_value(std::string_view key) const {
  if (key == "dir") {
    return this->server.dir;
  } else if (key == "dbfilename") {
    return this->server.dbfilename;
  }
  
  return {};
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

Server::Server(EventLoopPtr event_loop, ServerInfo info)
    : _event_loop(event_loop)
    , _info(std::move(info))
{
  this->_is_replica = this->_info.replication.role == "slave";

  this->_new_server_fd_signal = std::make_shared<Signal<int, PollEventTypeList, SignalPtr<PollEventType>>>();
  this->_removed_server_fd_signal = std::make_shared<Signal<int>>();
  this->_new_fd_signal = std::make_shared<Signal<int>>();
  this->_removed_fd_signal = std::make_shared<Signal<int>>();

  this->_fd_event_signal = std::make_shared<Signal<PollEventType>>();
  this->_slot_fd_event = std::make_shared<Slot<PollEventType>>([this](PollEventType type) {
    if (type != PollEventType::ReadyToRead) {
      throw std::runtime_error("Server got unexpected poll event");
      // TODO make error more verbose
    }

    try {
      while (auto maybe_client_fd = this->accept()) {
        this->_new_fd_signal->emit(maybe_client_fd.value());
      }
    } catch (std::exception& e) {
      std::cerr << "Client accepting error: " << e.what() << std::endl;
    }
  });
  this->_fd_event_signal->connect(this->_slot_fd_event);

  this->_start_handle = this->_event_loop->post([this]() {
    this->start();
  });
}

Server::~Server() {
  this->close();
}

SignalPtr<int, PollEventTypeList, SignalPtr<PollEventType>>& Server::new_server_fd() {
  return this->_new_server_fd_signal;
}

SignalPtr<int>& Server::removed_server_fd() {
  return this->_removed_server_fd_signal;
}

SignalPtr<int>& Server::new_fd() {
  return this->_new_fd_signal;
}

SignalPtr<int>& Server::removed_fd() {
  return this->_removed_fd_signal;
}

bool Server::is_replica() const {
  return this->_is_replica;
}

void Server::start() {
  if (DEBUG_LEVEL >= 1) std::cerr << "DEBUG Server starting on 0.0.0.0:" << this->_info.server.tcp_port << std::endl;

  int server_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
  if (server_fd < 0) {
    std::ostringstream ss;
    ss << "Failed to create server socket: " << strerror(errno);
    throw std::runtime_error(ss.str());
  }

  this->_server_fd = server_fd;

  int reuse = 1;
  if (setsockopt(*this->_server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    std::ostringstream ss;
    ss << "Setsockopt failed: " << strerror(errno);
    throw std::runtime_error(ss.str());
  }

  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(this->_info.server.tcp_port);

  bool binded = false;
  const int retry_ms = 250;
  const int max_tries = 8;
  for (std::size_t tries = 0; tries < max_tries; ++tries) {
    if (bind(*this->_server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
      if (errno != EADDRINUSE) {
        break;
      }
      if (DEBUG_LEVEL >= 2) std::cerr << "DEBUG Failed to bind, address in use, retrying in " << retry_ms << "ms, try #" << tries + 1 << std::endl;
      usleep(retry_ms * 1000);
    } else {
      binded = true;
      break;
    }
  }

  if (!binded) {
    std::ostringstream ss;
    ss << "Failed to bind to port " << this->_info.server.tcp_port << ": " << strerror(errno);
    throw std::runtime_error(ss.str());
  }

  if (listen(*this->_server_fd, CONN_BACKLOG) != 0) {
    std::ostringstream ss;
    ss << "Listen failed: " << strerror(errno);
    throw std::runtime_error(ss.str());
  }

  this->_new_server_fd_signal->emit(
      this->_server_fd.value(),
      PollEventTypeList{PollEventType::ReadyToRead},
      this->_fd_event_signal);
  
  if (DEBUG_LEVEL >= 1) std::cerr << "DEBUG Server ready!" << std::endl;
}

std::optional<int> Server::accept() {
  struct sockaddr_in client_addr;
  std::size_t client_addr_len = sizeof(client_addr);

  int accept_res = ::accept(this->_server_fd.value(), (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_len);
  if (accept_res < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return {};
    } else {
      std::ostringstream ss;
      ss << "Server got error on accept: errno=" << errno;
      throw std::runtime_error(ss.str());
    }
  }

  return accept_res;
}

ServerInfo& Server::info() {
  return this->_info;
}

void Server::close() {
  if (this->_server_fd) {
    this->_removed_server_fd_signal->emit(this->_server_fd.value());
    ::close(this->_server_fd.value());
    this->_server_fd.reset();
  }
}
