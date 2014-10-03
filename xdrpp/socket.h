// -*- C++ -*-

#ifndef _XDRPP_SOCKET_H_HEADER_INCLUDED_
#define _XDRPP_SOCKET_H_HEADER_INCLUDED_ 1

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

//! A deleter to use \c std::unique_ptr with \c addrinfo structures
//! (which must be freed recursively).
struct delete_addrinfo {
  constexpr delete_addrinfo() {}
  void operator()(addrinfo *ai) const { freeaddrinfo(ai); }
};
//! Automatically garbage-collected addrinfo pointer.
using unique_addrinfo = std::unique_ptr<addrinfo, delete_addrinfo>;

//! Wrapper around \c getaddrinfo that returns a garbage-collected
//! xdr::unique_addrinfo.  \throws std::system_error with
//! xdr::gai_category on failure.
unique_addrinfo get_addrinfo(const char *host,
			     int socktype = SOCK_STREAM,
			     const char *service = nullptr,
			     int family = AF_UNSPEC);

//! Return printable versions of numeric host and port number
void get_numinfo(const sockaddr *sa, socklen_t salen,
		 std::string *host, std::string *serv);

//! Self-closing file descriptor.  Note that this file descriptor will
//! be closed as soon as it goes out of scope, hence it is important
//! to see whether functions you pass it to take a "unique_fd &" or a
//! "const unique_fd &&".  In the latter case, you are expected to
//! keep the file descriptor around.
class unique_fd {
  int fd_;
public:
  unique_fd() : unique_fd(-1) {}
  explicit unique_fd(int fd) : fd_(fd) {}
  unique_fd(unique_fd &&uf) : fd_(uf.release()) {}
  ~unique_fd() { clear(); }
  unique_fd &operator=(unique_fd &&uf) {
    clear();
    fd_ = uf.release();
    return *this;
  }

  //! Return the file descriptor number, but maintain ownership.
  int get() const { return fd_; }
  //! True if the file descriptor is not -1.
  explicit operator bool() const { return fd_ != -1; }
  //! Return the file descriptor number, relinquishing ownership of
  //! it.  The \c unique_fd will have file descriptor -1 after this
  //! method returns.
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
  void reset(int fd) { clear(); fd_ = fd; }
};

//! Try connecting to the first \b addrinfo in a linked list.
unique_fd tcp_connect1(const addrinfo *ai, bool ndelay = false);

//! Try connecting to every \b addrinfo in a list until one succeeds.
unique_fd tcp_connect(const addrinfo *ai);
inline unique_fd
tcp_connect(const unique_addrinfo &ai)
{
  return tcp_connect(ai.get());
}
unique_fd tcp_connect(const char *host, const char *service,
		      int family = AF_UNSPEC);

//! Create a TCP connection to an RPC server on \c host, first
//! querying \c rpcbind on \c host to determine the port.
unique_fd tcp_connect_rpc(const char *host,
			  std::uint32_t prog, std::uint32_t vers,
			  int family = AF_UNSPEC);

//! Create and bind a TCP socket on which it is suitable to called \c
//! listen.  (Note that this function doesn't call \c listen itself,
//! but binds the socket so you can.)
unique_fd tcp_listen(const char *service = "0", int family = AF_UNSPEC);

//! Extract the port number from an RFC1833 / RFC5665 universal
//! network address (uaddr).
int parse_uaddr_port(const std::string &uaddr);

//! Create a uaddr for a local address or file descriptor.
std::string make_uaddr(const sockaddr *sa, socklen_t salen);
std::string make_uaddr(int fd);
//! Register a service listening on \c sa with \c rpcbind.
void rpcbind_register(const sockaddr *sa, socklen_t salen,
		      std::uint32_t prog, std::uint32_t vers);
void rpcbind_register(int fd, std::uint32_t prog, std::uint32_t vers);

}

#endif // !_XDRPP_SOCKET_H_HEADER_INCLUDED_
