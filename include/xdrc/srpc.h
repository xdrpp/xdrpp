// -*- C++ -*-

//! \file srpc.h Simple synchronous RPC functions.

#include <xdrc/message.h>
#include <mutex>
#include <condition_variable>

namespace xdr {

msg_ptr read_message(int fd);
void write_message(int fd, const msg_ptr &m);

//! Synchronous file descriptor demultiplexer.
class synchronous_client {
  const int fd_;
  std::uint32_t xid_{1};
public:
  synchronous_client(int fd) : fd_(fd) {}

};

}
