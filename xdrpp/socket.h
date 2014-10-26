// -*- C++ -*-

#ifndef _XDRPP_SOCKET_H_HEADER_INCLUDED_
#define _XDRPP_SOCKET_H_HEADER_INCLUDED_ 1

//! \file socket.h Simplified support for creatins sockets.

#include <memory>
#include <system_error>
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>

namespace xdr {

//! Abstract away the type of a socket (for windows).
struct sock_t {
  int fd_;

  constexpr sock_t() : fd_{-1} {}
  constexpr explicit sock_t(int fd) : fd_(fd) {}

  bool operator==(sock_t s) const { return fd_ == s.fd_; }
  bool operator<(sock_t s) const { return fd_ < s.fd_; }

  ssize_t read(void *buf, std::size_t count) const {
    return ::read(fd_, buf, count);
  }
  ssize_t readv(const struct iovec *iov, int iovcnt) const {
    return ::readv(fd_, iov, iovcnt);
  }
  ssize_t write(const void *buf, std::size_t count) const {
    return ::write(fd_, buf, count);
  }
  ssize_t writev(const struct iovec *iov, int iovcnt) const {
    return ::writev(fd_, iov, iovcnt);
  }
  void close() const;
};
constexpr sock_t invalid_sock;

} namespace std {
template<> struct hash<xdr::sock_t> {
  using argument_type = xdr::sock_t;
  using result_type = size_t;
  constexpr hash() {}
  size_t operator()(const xdr::sock_t s) const { return s.fd_; }
};
} namespace xdr {

inline bool
operator!=(sock_t s1, sock_t s2) {
  return !(s1 == s2);
}

//! Last socket error message (\c strerror(errno) on POSIX).
const char *sock_errmsg();

//! Throw a \c system_error exception for the last socket error.
[[noreturn]] void throw_sockerr(const char *);

//! Set the \c O_NONBLOCK flag on a socket.  \throws std::system_error
//! on failure.
void set_nonblock(sock_t s);

//! Set the close-on-exec flag of a file descriptor.  \throws
//! std::system_error on failure.
void set_close_on_exec(sock_t s);

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

//! Self-closing socket.  Note that this socket will be closed as soon
//! as it goes out of scope, hence it is important to see whether
//! functions you pass it to take a "unique_sock &" or a "const
//! unique_sock &&".  In the latter case, you are expected to keep the
//! file descriptor around.
class unique_sock {
  sock_t s_;
public:
  unique_sock() : s_(invalid_sock) {}
  explicit unique_sock(sock_t s) : s_(s) {}
  unique_sock(unique_sock &&uf) : s_(uf.release()) {}
  ~unique_sock() { clear(); }
  unique_sock &operator=(unique_sock &&uf) {
    clear();
    s_ = uf.release();
    return *this;
  }

  //! Return the file descriptor number, but maintain ownership.
  sock_t get() const { return s_; }
  //! True if the file descriptor is not -1.
  explicit operator bool() const { return s_ != invalid_sock; }
  //! Return the file descriptor number, relinquishing ownership of
  //! it.  The \c unique_sock will have file descriptor -1 after this
  //! method returns.
  sock_t release() {
    sock_t ret = s_;
    s_ = invalid_sock;
    return ret;
  }
  void clear() {
    if (s_ != invalid_sock) {
      s_.close();
      s_ = invalid_sock;
    }
  }
  void reset(sock_t s) { clear(); s_ = s; }
};

//! Try connecting to the first \b addrinfo in a linked list.
unique_sock tcp_connect1(const addrinfo *ai, bool ndelay = false);

//! Try connecting to every \b addrinfo in a list until one succeeds.
unique_sock tcp_connect(const addrinfo *ai);
inline unique_sock
tcp_connect(const unique_addrinfo &ai)
{
  return tcp_connect(ai.get());
}
unique_sock tcp_connect(const char *host, const char *service,
			int family = AF_UNSPEC);

//! Create a TCP connection to an RPC server on \c host, first
//! querying \c rpcbind on \c host to determine the port.
unique_sock tcp_connect_rpc(const char *host,
			    std::uint32_t prog, std::uint32_t vers,
			    int family = AF_UNSPEC);

//! Create and bind a TCP socket on which it is suitable to called \c
//! listen.  (Note that this function doesn't call \c listen itself,
//! but binds the socket so you can.)
unique_sock tcp_listen(const char *service = "0", int family = AF_UNSPEC);

//! Extract the port number from an RFC1833 / RFC5665 universal
//! network address (uaddr).
int parse_uaddr_port(const std::string &uaddr);

//! Create a uaddr for a local address or file descriptor.
std::string make_uaddr(const sockaddr *sa, socklen_t salen);
std::string make_uaddr(sock_t s);
//! Register a service listening on \c sa with \c rpcbind.
void rpcbind_register(const sockaddr *sa, socklen_t salen,
		      std::uint32_t prog, std::uint32_t vers);
void rpcbind_register(sock_t s, std::uint32_t prog, std::uint32_t vers);

//! Wrapper around accept for sock_t.
sock_t accept(sock_t s, sockaddr *addr, socklen_t *addrlen);

//! Create a socket (or pipe on unix, where both are file descriptors)
//! that is connected to itself.
void create_selfpipe(sock_t ss[2]);

}

#endif // !_XDRPP_SOCKET_H_HEADER_INCLUDED_
