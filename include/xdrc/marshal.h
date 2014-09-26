// -*- C++ -*-

#ifndef _XDRC_MARSHAL_H_HEADER_INCLUDED_
#define _XDRC_MARSHAL_H_HEADER_INCLUDED_ 1

#include <cstdint>
#include <ostream>
#include <type_traits>
#include <sys/uio.h>
#include <xdrc/endian.h>
#include <xdrc/types.h>

namespace xdr {

struct marshal_base {
  struct u64conv {
    union {
      std::uint64_t u64;
      std::uint32_t u32[2];
    };
    u64conv(std::uint64_t u) : u64(u) {}
  };

  static void getBytes(const std::uint32_t *&pr, void *buf, std::size_t len) {
    const char *p = reinterpret_cast<const char *>(pr);
    memcpy(buf, p, len);
    p += len;
    while (len & 3) {
      ++len;
      if (*p++ != '\0')
	throw xdr_should_be_zero("Non-zero padding bytes encountered");
    }
    pr = reinterpret_cast<const std::uint32_t *>(p);
  }
  static void putBytes(std::uint32_t *&pr, const void *buf, std::size_t len) {
    char *p = reinterpret_cast<char *>(pr);
    memcpy(p, buf, len);
    p += len;
    while (len & 3) {
      ++len;
      *p++ = '\0';
    }
    pr = reinterpret_cast<std::uint32_t *>(p);
  }
		       
};

struct marshal_noswap : marshal_base {
  static void put32(std::uint32_t *&p, std::uint32_t v) { *p++ = v; }
  static void put64(std::uint32_t *&p, u64conv u) {
    *p++ = u.u32[0];
    *p++ = u.u32[1];
  }
};

struct marshal_swap : marshal_base {
  static void put32(std::uint32_t *&p, std::uint32_t v) {
    *p++ = swap32(v);
  }
  static void put64(std::uint32_t *&p, u64conv u) {
    *p++ = swap32(u.u32[1]);
    *p++ = swap32(u.u32[0]);
  }
};

#if WORDS_BIGENDIAN
using marshal_be = marshal_noswap;
using marshal_le = marshal_swap;
#else // !WORDS_BIGENDIAN
using marshal_be = marshal_swap;
using marshal_le = marshal_noswap;
#endif // !WORDS_BIGENDIAN



}

#endif // !_XDRC_MARSHAL_H_HEADER_INCLUDED_

