// -*- C++ -*-

#ifndef _XDRC_MESSAGE_H_HEADER_INCLUDED_
#define _XDRC_MESSAGE_H_HEADER_INCLUDED_ 1

#include <cassert>
#include <cstdint>
#include <memory>
#include <stddef.h>
#include <xdrc/endian.h>

//! \file msgbuf.h Message buffer with space for marshaled length.

namespace xdr {

class message_t {
  const std::size_t size_;
  alignas(std::uint32_t) char buf_[4];
  message_t(std::size_t size) : size_(size) {}
public:
  void *data() { return buf_ + 4; }
  const void *data() const { return buf_ + 4; }
  std::size_t size() const { return size_; }
  void *end() { return buf_ + 4 + size_; }
  const void *end() const { return buf_ + 4 + size_; }

  //! 4-byte size in network byte order, followed by data.
  std::uint32_t *raw_data() {
    return reinterpret_cast<std::uint32_t *>(buf_);
  }
  const std::uint32_t *raw_data() const {
    return reinterpret_cast<const std::uint32_t *>(buf_);
  }
  //! Size 4-byte length plus data.
  std::size_t raw_size() const { return size_ + 4; }

  static message_t *alloc(std::size_t size) {
    // In RPC, the high bit means a continuation packet follows, which
    // we don't implement but reserve for future compatibility.
    assert(size < 0x80000000);
    void *raw = operator new(offsetof(message_t, buf_[size + 4]));
    message_t *m = new (raw) message_t (size);
    *m->raw_data() = swap32le(size);
    return m;
  }
};

using msg_ptr = std::unique_ptr<message_t>;

}

#endif // !_XDRC_MESSAGE_H_HEADER_INCLUDED_
