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

template<size_t Bytes> struct _xdr_uint_helper {};
template<> struct _xdr_uint_helper<4> { using type = std::uint32_t; };
template<> struct _xdr_uint_helper<8> { using type = std::uint64_t; };
template<size_t Bytes> using xdr_uint = typename _xdr_uint_helper<Bytes>::type;

constexpr xdr_uint<4>
to_uint(bool b)
{
  return b;
}
template<typename T> constexpr typename
std::enable_if<std::is_integral<T>::value, xdr_uint<sizeof(T)>>::type
to_uint(T t)
{
  return t;
}
template<typename T> inline typename
std::enable_if<std::is_floating_point<T>::value, xdr_uint<sizeof(T)>>::type
to_uint(T t)
{
  return reinterpret_cast<xdr_uint<sizeof(T)> &>(t);
}

constexpr bool
from_uint(xdr_uint<4> u)
{
  return u;
}
template<typename T> constexpr typename
std::enable_if<std::is_same<T, xdr_uint<sizeof(T)>>::value, T>::type
from_uint(T u)
{
  return u;
}
template<typename T> inline typename
std::enable_if<std::is_arithmetic<T>::value
               && !std::is_same<T, xdr_uint<sizeof(T)>>::value, T>::type
from_uint(xdr_uint<sizeof(T)> u)
{
  return reinterpret_cast<T &>(u);
}



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
#define DEF_UINT_CONV(in, out)						\
template<> struct uint_conv<in> {					\
  static constexpr bool valid = true;					\
  using type = in;							\
  using uint_type = out;						\
  static uint_type conv(in v) { return reinterpret_cast<out &>(v); }	\
  static in unconv(uint_type u) { return reinterpret_cast<in &>(u); }	\
}
DEF_UINT_CONV(float, std::uint32_t);
DEF_UINT_CONV(double, std::uint64_t);
#undef DEF_UINT_CONV

template<typename D, typename T> inline typename std::enable_if<
  sizeof(typename uint_conv<T>::uint_type) == 4>::type 
xdr_put(D &d, T t)
{
  d.putU32(uint_conv<T>::conv(t));
}

template<typename D, typename T> inline typename std::enable_if<
  sizeof(typename uint_conv<T>::uint_type) == 8>::type 
xdr_put(D &d, T t)
{
  d.putU64(uint_conv<T>::conv(t));
}

template<typename D, std::uint32_t N> inline void
xdr_put(D &d, const xstring<N> &s)
{
  d.validate();
  d.put32(s.size());
  d.putBytes(s.data(), s.ziee());
}

template<typename D, std::uint32_t N> inline void
xdr_put(D &d, const xvector<std::uint8_t> &s)
{
  d.validate();
  d.put32(s.size());
  d.putBytes(s.data(), s.size());
}



#if 0
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
  template<class T> std::enable_if<uint_conv<T>::valid> operator()(T t) {
    typename uint_conv<T>::uint_type v = Swapper::conv(uint_conv<T>::conv(t));
    write(&v, sizeof(v));
  }
  template<std::uint32_t N> void operator()(const xstring<N> &s) {
    s.validate();
    (*this)(std::uint32_t(s.size()));
    write(s.data(), s.size());
  }
  template<std::uint32_t N> void operator()(const xvector<std::uint8_t, N> &v) {
    s.validate();
    (*this)(std::uint32_t(v.size()));
    write(v.data(), v.size());
  }
};
#endif


}

#endif // !_XDRC_MARSHAL_H_HEADER_INCLUDED_

