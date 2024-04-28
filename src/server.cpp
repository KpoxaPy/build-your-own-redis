#include "server.h"

#include <arpa/inet.h>
#include <cstdlib>
#include <cstring>
#include <netdb.h>
#include <sstream>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

Server::Server() {
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
  server_addr.sin_port = htons(Server::PORT);

  if (bind(*this->_server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
    std::ostringstream ss;
    ss << "Failed to bind to port " << Server::PORT;
    throw std::runtime_error(ss.str());
  }

  if (listen(*this->_server_fd, Server::CONN_BACKLOG) != 0) {
    throw std::runtime_error("Listen failed");
  }
}

Server::~Server() {
  this->close();
}

std::optional<Client> Server::accept() {
  struct sockaddr_in client_addr;
  std::size_t client_addr_len = sizeof(client_addr);

  int client_fd = ::accept(*this->_server_fd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_len);
  if (client_fd < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return {};
    } else {
      throw std::runtime_error("Failed to accept new connection");
    }
  }

  return std::move(Client(client_fd));
}

void Server::close() {
  if (this->_server_fd) {
    ::close(this->_server_fd.value());
    this->_server_fd.reset();
  }
}
