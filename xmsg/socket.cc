
#include <cstring>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <xdrc/socket.h>

namespace xdr {

using std::string;

void
set_nonblock(int fd)
{
  int n;
  if ((n = fcntl (fd, F_GETFL)) == -1
      || fcntl (fd, F_SETFL, n | O_NONBLOCK) == -1)
    throw std::system_error(errno, std::system_category(), "O_NONBLOCK");
}

void
set_close_on_exec(int fd)
{
  int n;
  if ((n = fcntl (fd, F_GETFD)) == -1
      || fcntl (fd, F_SETFD, n | FD_CLOEXEC) == -1)
    throw std::system_error(errno, std::system_category(), "F_SETFD");
}

void
really_close(int fd)
{
  while (close(fd) == -1)
    if (errno != EINTR) {
      std::cerr << "really_close: " << std::strerror(errno) << std::endl;
      return;
    }
}

struct gai_category_impl : public std::error_category {
  const char *name() const noexcept override { return "DNS"; }
  string message(int err) const override { return gai_strerror(err); }
};

const std::error_category &
gai_category()
{
  static gai_category_impl cat;
  return cat;
}

namespace {

string
cat_host_service(const char *host, const char *service)
{
  string target;
  if (host) {
    if (std::strchr(host, ':')) {
      target += "[";
      target += host;
      target += "]";
    }
    else
      target = host;
  }
  if (service) {
    target += ":";
    target += service;
  }
  return target;
}

}

unique_addrinfo get_addrinfo(const char *host, int socktype,
			     const char *service, int family)
{
  addrinfo hints, *res;
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_socktype = socktype;
  hints.ai_family = family;
  hints.ai_flags = AI_ADDRCONFIG;
  int err = getaddrinfo(host, service, &hints, &res);
  if (!err)
    return unique_addrinfo(res);
  throw std::system_error(err, gai_category(), cat_host_service(host, service));
}

unique_fd
tcp_connect(const char *host, const char *service)
{
  unique_addrinfo ai = get_addrinfo(host, SOCK_STREAM, service);
  int fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
  if (fd == -1)
    throw std::system_error(errno, std::system_category(), "socket");
  if (connect(fd, ai->ai_addr, ai->ai_addrlen) == -1) {
    int saved_errno = errno;
    really_close(fd);
    throw std::system_error(saved_errno, std::system_category(),
			    cat_host_service(host, service));
  }
  return unique_fd{fd};
}

int
parse_uaddr_port(const string &uaddr)
{
  int low = uaddr.rfind('.');
  if (low == string::npos || low == 0)
    return -1;
  int high = uaddr.rfind('.', low - 1);
  if (high == string::npos)
    return -1;
}

}
