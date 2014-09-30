// -*- C++ -*-

#ifndef _XDRC_SOCKET_H_HEADER_INCLUDED_
#define _XDRC_SOCKET_H_HEADER_INCLUDED_ 1

//! \file socket.h Simplified support for creatins sockets.

#include <memory>
#include <system_error>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

namespace xdr {

//! Set the \c O_NONBLOCK flag on a socket.  \throws std::system_error
//! on failure.
void set_nonblock(int fd);

//! Set the close-on-exec flag of a file descriptor.  \throws
//! std::system_error on failure.
void set_close_on_exec(int fd);

//! Keep closing a file descriptor until you don't get \c EINTR.
void really_close(int fd);

//! Category for system errors dealing with DNS (getaddrinfo, etc.).
const std::error_category &gai_category();

struct delete_addrinfo {
  constexpr delete_addrinfo() {}
  void operator()(addrinfo *ai) const { freeaddrinfo(ai); }
};
//! Automatically garbage-collected addrinfo pointer.
using unique_addrinfo = std::unique_ptr<addrinfo, delete_addrinfo>;

unique_addrinfo get_addrinfo(const char *host,
			     int socktype = SOCK_STREAM,
			     const char *service = nullptr,
			     int family = AF_UNSPEC);

unique_addrinfo get_rpcaddr(const char *host, std::uint32_t prog,
			    std::uint32_t vers, int family = AF_UNSPEC);

//! Return printable versions of numeric host and port number
void get_numinfo(const sockaddr *sa, socklen_t salen,
		 std::string *host, std::string *serv);

//! Self-closing file descriptor.
class unique_fd {
  int fd_;
public:
  explicit unique_fd(int fd) : fd_(fd) {}
  unique_fd(unique_fd &&uf) { fd_ = uf.fd_; uf.fd_ = -1; }
  ~unique_fd() { clear(); }
  unique_fd &operator=(unique_fd &&uf) {
    clear();
    std::swap(fd_, uf.fd_);
    return *this;
  }

  int get() const { return fd_; }
  operator int() const { return fd_; }
  explicit operator bool() const { return fd_ != -1; }
  int release() {
    int ret = fd_;
    fd_ = -1;
    return ret;
  }
  void clear() {
    if (fd_ != -1) {
      really_close(fd_);
      fd_ = -1;
    }
  }
};

unique_fd tcp_connect(const addrinfo *ai);
inline unique_fd
tcp_connect(const unique_addrinfo &ai)
{
  return tcp_connect(ai.get());
}
unique_fd tcp_connect(const char *host, const char *service,
		      int family = AF_UNSPEC);
unique_fd tcp_connect_rpc(const char *host,
			  std::uint32_t prog, std::uint32_t vers,
			  int family = AF_UNSPEC);

unique_fd tcp_listen(const char *service = "0", int family = AF_UNSPEC);

int parse_uaddr_port(const std::string &uaddr);

std::string make_uaddr(const sockaddr *sa, socklen_t salen);
void rpcbind_register(const sockaddr *sa, socklen_t salen,
		      std::uint32_t prog, std::uint32_t vers);
void rpcbind_register(int fd, std::uint32_t prog, std::uint32_t vers);

}

#endif // !_XDRC_SOCKET_H_HEADER_INCLUDED_
