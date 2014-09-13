// -*- C++ -*-

#ifndef _XDRC_MARSHAL_H_HEADER_INCLUDED_
#define _XDRC_MARSHAL_H_HEADER_INCLUDED_ 1

#include <cstdint>
#include <ostream>
#include <type_traits>
#include <sys/uio.h>
#include <xdrc/endian.h>
#include <xdrc/types.h>

namespace xdrc {

template<typename T> struct uint_conv {
  static constexpr bool valid = false;
};
template<> struct uint_conv<bool> {
  static constexpr bool valid = true;
  using type = bool;
  using uint_type = std::uint32_t;
  static constexpr uint_type conv(bool v) { return v; }
  static constexpr bool unconv(uint_type v) { return v; }
};

//! \hideinitializer
#define DEF_UINT_CONV(in, out)				\
template<> struct uint_conv<in> {			\
  static constexpr bool valid = true;			\
  using type = in;					\
  using uint_type = out;				\
  static constexpr uint_type conv(in v) { return v; }	\
  static in unconv(uint_type v) {			\
    return reinterpret_cast<in &>(v);			\
  }							\
}
DEF_UINT_CONV(std::int32_t, std::uint32_t);
DEF_UINT_CONV(std::uint32_t, std::uint32_t);
DEF_UINT_CONV(std::int64_t, std::uint32_t);
DEF_UINT_CONV(std::uint64_t, std::uint64_t);
#undef DEF_UINT_CONV

//! \hideinitializer
#define DEF_UINT_CONV(in, out)			\
template<> struct uint_conv<in> {		\
  static constexpr bool valid = true;		\
  using type = in;				\
  using uint_type = out;			\
  static uint_type conv(in v) {			\
    union {					\
      in vv;					\
      uint_type uu;				\
    };						\
    vv = v;					\
    return uu;					\
  }						\
  static in unconv(uint_type u) {		\
    union {					\
      in vv;					\
      uint_type uu;				\
    };						\
    uu = u;					\
    return vv;					\
  }						\
}
DEF_UINT_CONV(float, std::uint32_t);
DEF_UINT_CONV(double, std::uint64_t);
#undef DEF_UINT_CONV

struct noswap_uint {
  static constexpr std::uint32_t conv(std::uint32_t v) { return v; }
  static constexpr std::uint64_t conv(std::uint64_t v) { return v; }
};

struct swap_uint {
  static constexpr std::uint32_t conv(std::uint32_t v) {
    return v << 24 | (v & 0xff00) << 8 | (v >> 8 & 0xff00) | v >> 24;
  }
  static constexpr std::uint64_t conv(std::uint64_t v) {
    return (std::uint64_t(conv(std::uint32_t(v))) << 32
	    | conv(std::uint32_t(v >> 32)));
  }
};

template<class Swapper> struct Put {
  std::ostream &os_;
  Put(std::ostream &os) : os_(os) {}

  void write(const void *buf, size_t len) {
    os_.write(static_cast<const char *>(buf), len);
    while (len & 3) {
      ++len;
      os_.put('\0');
    }
  }
  template<class T> std::enable_if<uint_conv<T>::valid> operator() (T t) {
    typename uint_conv<T>::uint_type v = Swapper::conv(uint_conv<T>::conv(t));
    write(&v, sizeof(v));
  }
};


}

#endif // !_XDRC_MARSHAL_H_HEADER_INCLUDED_

