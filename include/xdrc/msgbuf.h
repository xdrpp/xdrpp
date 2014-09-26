// -*- C++ -*-

#ifndef _XDRC_MSGBUF_H_HEADER_INCLUDED_
#define _XDRC_MSGBUF_H_HEADER_INCLUDED_ 1

#include <cassert>
#include <cstdint>
#include <memory>
#include <stddef.h>
#include <xdrc/endian.h>

//! \file msgbuf.h Message buffer with space for marshaled length.

namespace xdr {

class msg_buf {
  std::uint32_t buf_[2];
  msg_buf() = default;
public:
  std::uint32_t size() const { return buf_[0]; }
  //! Field for holding size in network byte order
  std::uint32_t &be_size() { return buf_[1]; }
  std::uint32_t *begin() { return &buf_[2]; }
  const std::uint32_t *begin() const { return &buf_[2]; }
  std::uint32_t *end() { return &buf_[2 + (size()>>2)]; }
  const std::uint32_t *end() const { return &buf_[2 + (size()>>2)]; }

  //! rawData includes the marshalled be_size field.
  const void *rawData() const { return &buf_[1]; }
  std::size_t rawSize() const { return std::size_t(4) + size(); }

  static msg_buf *alloc(std::uint32_t s) {
    assert(s < 0x80000000 && !(s&3));
    void *raw = operator new(offsetof(msg_buf, buf_[(s + std::size_t(11))>>2]));
    msg_buf *b = new (raw) msg_buf;
    b->buf_[0] = s;
    b->buf_[1] = swap32le(s);
    return b;
  }
};

using msg_ptr = std::unique_ptr<msg_buf>;

}

#endif // !_XDRC_MSGBUF_H_HEADER_INCLUDED_
