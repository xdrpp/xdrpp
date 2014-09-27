// -*- C++ -*-

#ifndef _XDRC_MESSAGE_H_HEADER_INCLUDED_
#define _XDRC_MESSAGE_H_HEADER_INCLUDED_ 1

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <xdrc/endian.h>

//! \file msgbuf.h Message buffer with space for marshaled length.

namespace xdr {

class message_t;
using msg_ptr = std::unique_ptr<message_t>;

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

  //! 4-byte size in network byte order, followed by data.
  char *raw_data() { return buf_; }
  const char *raw_data() const { return buf_; }
  //! Size 4-byte length plus data.
  std::size_t raw_size() const { return size_ + 4; }

  static msg_ptr alloc(std::size_t size);
};

}

#endif // !_XDRC_MESSAGE_H_HEADER_INCLUDED_
