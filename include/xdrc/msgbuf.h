// -*- C++ -*-

#ifndef _MSGBUF_H_
#define _MSGBUF_H_ 1

#include <cstdint>
#include <memory>
#include <stddef.h>
#include <xdrc/endian.h>

//! \file msgbuf.h Message buffer with space for marshaled length.

namespace xdr {

class MsgBuf {
protected:
  MsgBuf(std::uint32_t s) : size(s) {}
public:
  const std::uint32_t size;
  std::uint32_t marshaled_size;
  std::uint32_t data[1];

  const void *rawData() const { return &marshaled_size; }
  const std::size_t rawSize() const { return std::size_t(4) + size; }

  static MsgBuf *alloc(std::uint32_t s) {
    void *raw = operator new(offsetof(MsgBuf, data[(s + std::size_t(3))>>2]));
    MsgBuf *b = new (raw) MsgBuf(s);
    b->marshaled_size = swap32le(s);
    return b;
  }
};

using MsgPtr = std::unique_ptr<MsgBuf>;

}

#endif // !_MSGBUF_H_
