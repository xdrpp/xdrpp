// -*- C++ -*-

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

struct delete_addrinfo {
  constexpr delete_addrinfo() {}
  void operator()(addrinfo *ai) const { freeaddrinfo(ai); }
};
//! Automatically garbage-collected addrinfo pointer.
using unique_addrinfo = std::unique_ptr<addrinfo, delete_addrinfo>;

}
