// -*- C++ -*-

#ifndef _XDRPP_MESSAGE_H_HEADER_INCLUDED_
#define _XDRPP_MESSAGE_H_HEADER_INCLUDED_ 1

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <xdrpp/endian.h>

//! \file message.h Message buffer with space for marshaled length.

namespace xdr {

class message_t;
using msg_ptr = std::unique_ptr<message_t>;

//! Fixed-size message buffer, with room at beginning for 4-byte length.
class message_t {
  const std::size_t size_;
  alignas(std::uint32_t) char buf_[4];
  message_t(std::size_t size) : size_(size) {}
public:
  std::size_t size() const { return size_; }
  char *data() { return buf_ + 4; }
  const char *data() const { return buf_ + 4; }
  const void *offset(std::ptrdiff_t i) const { return buf_ + i; }
  //! End of the buffer (one past last byte).
  char *end() { return buf_ + 4 + size_; }
  const char *end() const { return buf_ + 4 + size_; }

  //! 4-byte buffer to store size in network byte order, followed by data.
  char *raw_data() { return buf_; }
  const char *raw_data() const { return buf_; }
  //! Size of 4-byte length plus data.
  std::size_t raw_size() const { return size_ + 4; }

  //! Allocate a new buffer.
  static msg_ptr alloc(std::size_t size);
};

}

#endif // !_XDRPP_MESSAGE_H_HEADER_INCLUDED_
