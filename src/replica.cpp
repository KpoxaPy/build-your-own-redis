#include "replica.h"

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <sstream>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>

std::string addr_to_string(const struct addrinfo* info) {
  void *addr;
  const char *ipver;
  char ipstr[INET6_ADDRSTRLEN];

  if (info->ai_family == AF_INET) {  // IPv4
    struct sockaddr_in *ipv4 = (struct sockaddr_in *)info->ai_addr;
    addr = &(ipv4->sin_addr);
    ipver = "IPv4";
  } else {  // IPv6
    struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)info->ai_addr;
    addr = &(ipv6->sin6_addr);
    ipver = "IPv6";
  }

  inet_ntop(info->ai_family, addr, ipstr, sizeof ipstr);
  
  return ipstr;
}

Replica::Replica(EventLoopPtr event_loop) 
  : _event_loop(event_loop)
{
  this->_new_fd_signal = std::make_shared<Signal<int, PollEventTypeList, SignalPtr<PollEventType>>>();
  this->_removed_fd_signal = std::make_shared<Signal<int>>();

  this->_start_handle =  this->_event_loop->post([this]() {
    this->start();
  });
}

void Replica::set_server(ServerPtr server) {
  this->_server = server;
}

void Replica::set_storage(IStoragePtr storage) {
  this->_storage = storage;
}

SignalPtr<int, PollEventTypeList, SignalPtr<PollEventType>> &Replica::new_fd() {
  return this->_new_fd_signal;
}

SignalPtr<int> &Replica::removed_fd() {
  return this->_removed_fd_signal;
}

void Replica::start() {
  struct addrinfo hints, *master_address;

  std::memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  auto port_str = std::to_string(this->_server->info().replication.master_port);
  auto gai_result = getaddrinfo(
      this->_server->info().replication.master_host.data(),
      port_str.data(), &hints, &master_address);
  
  if (gai_result != 0) {
    std::ostringstream ss;
    ss << "Getaddrinfo for master host resulted in error: " << gai_strerror(gai_result);
    throw std::runtime_error(ss.str());
  }

  if (master_address == nullptr) {
    std::ostringstream ss;
    ss << "Cannot find address for master host: " << this->_server->info().replication.master_host;
    throw std::runtime_error(ss.str());
  }

  int master_fd = socket(master_address->ai_family, SOCK_STREAM, 0);
  if (master_fd < 0) {
    std::ostringstream ss;
    ss << "Failed to create socket for connection to master, error: " << strerror(errno);
    throw std::runtime_error(ss.str());
  }
  this->_master_fd = master_fd;

  auto connect_result = connect(this->_master_fd.value(), master_address->ai_addr, master_address->ai_addrlen);

  if (connect_result != 0) {
    std::ostringstream ss;
    ss << "Cannot connect to master, error: " << strerror(errno);
    throw std::runtime_error(ss.str());
  }

  freeaddrinfo(master_address);

  this->_talker = std::make_shared<ReplicaTalker>();
  this->_talker->set_server(this->_server);
  this->_talker->set_storage(this->_storage);

  this->_handler = std::make_unique<Handler>(this->_event_loop, this->_master_fd.value(), this->_talker);
  this->_handler->new_fd()->connect(this->_new_fd_signal);
  this->_handler->removed_fd()->connect(this->_removed_fd_signal);
}
