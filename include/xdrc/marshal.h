// -*- C++ -*-

#ifndef _XDRC_MARSHAL_H_HEADER_INCLUDED_
#define _XDRC_MARSHAL_H_HEADER_INCLUDED_ 1

#include <cstring>
#include <xdrc/endian.h>
#include <xdrc/message.h>
#include <xdrc/types.h>

namespace xdr {

struct marshal_base {
  struct u64conv {
    union {
      std::uint64_t u64;
      std::uint32_t u32[2];
    };
    u64conv() = default;
    u64conv(std::uint64_t u) : u64(u) {}
  };

  //! Copy \c len bytes to buf, then consume 0-3 bytes of padding to
  //! make the total number of bytes consumed divisible by 4.  \throws
  //! xdr_should_be_zero if the padding bytes are not zero.
  static void get_bytes(const std::uint32_t *&pr, void *buf, std::size_t len);
  //! Copy \c len bytes from buf, then add 0-3 zero-valued padding
  //! bytes to make the overall marshaled length a multiple of 4.
  static void put_bytes(std::uint32_t *&pr, const void *buf, std::size_t len);
};

struct marshal_noswap : marshal_base {
  static void put32(std::uint32_t *&p, std::uint32_t v) { *p++ = v; }
  static void put64(std::uint32_t *&p, u64conv u) {
    *p++ = u.u32[0];
    *p++ = u.u32[1];
  }

  static std::uint32_t get32(const std::uint32_t *&p) { return *p++; }
  static std::uint64_t get64(const std::uint32_t *&p) {
    u64conv u;
    u.u32[0] = *p++;
    u.u32[1] = *p++;
    return u.u64;
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
  static std::uint32_t get32(const std::uint32_t *&p) { return swap32(*p++); }
  static std::uint64_t get64(const std::uint32_t *&p) {
    u64conv u;
    u.u32[1] = swap32(*p++);
    u.u32[0] = swap32(*p++);
    return u.u64;
  }
};

template<typename Base> struct xdr_generic_put : Base {
  using Base::put32;
  using Base::put64;
  using Base::put_bytes;

  std::uint32_t *p_;
  std::uint32_t *const e_;

  xdr_generic_put() = default;
  xdr_generic_put(msg_ptr &m)
    : p_(reinterpret_cast<std::uint32_t *>(m->data())),
      e_(reinterpret_cast<std::uint32_t *>(m->end())) {
    // Assertion failure rather than exception, because we generated message.
    assert(!(m->size() & 3));
  }

  void check(std::size_t n) const {
    if (n > std::size_t(reinterpret_cast<char *>(e_)
			- reinterpret_cast<char *>(p_)))
      throw xdr_overflow("insufficient buffer space in xdr_generic_put");
  }

  template<typename T> typename std::enable_if<
    std::is_same<std::uint32_t, typename xdr_traits<T>::uint_type>::value>::type
  operator()(T t) { check(4); put32(p_, xdr_traits<T>::to_uint(t)); }

  template<typename T> typename std::enable_if<
    std::is_same<std::uint64_t, typename xdr_traits<T>::uint_type>::value>::type
  operator()(T t) { check(8); put64(p_, xdr_traits<T>::to_uint(t)); }

  template<typename T> typename std::enable_if<xdr_traits<T>::is_bytes>::type
  operator()(const T &t) {
    if (xdr_traits<T>::variable_nelem) {
      check(4 + t.size());
      put32(p_, t.size());
    }
    else
      check(t.size());
    put_bytes(p_, t.data(), t.size());
  }

  template<typename T> typename std::enable_if<
    xdr_traits<T>::is_class || xdr_traits<T>::is_container>::type
  operator()(const T &t) { xdr_traits<T>::save(*this, t); }
};

template<typename Base> struct xdr_generic_get : Base {
  using Base::get32;
  using Base::get64;
  using Base::get_bytes;

  const std::uint32_t *p_;
  const std::uint32_t *const e_;

  xdr_generic_get() = default;
  xdr_generic_get(const msg_ptr &m)
    : p_(reinterpret_cast<std::uint32_t *>(m->data())),
      e_(reinterpret_cast<std::uint32_t *>(m->end())) {
    if (m->size() & 3)
      throw xdr_bad_message_size("xdr_generic_get: message size not"
				 " multiple of 4");
  }

  void check(std::uint32_t n) const {
    if (reinterpret_cast<const char *>(e_)
	- reinterpret_cast<const char *>(p_) < n)
      throw xdr_overflow("insufficient buffer space in xdr_generic_get");
  }

  template<typename T> typename std::enable_if<
    std::is_same<std::uint32_t, typename xdr_traits<T>::uint_type>::value>::type
  operator()(T &t) { check(4); t = xdr_traits<T>::from_uint(get32(p_)); }

  template<typename T> typename std::enable_if<
    std::is_same<std::uint64_t, typename xdr_traits<T>::uint_type>::value>::type
  operator()(T &t) { check(8); t = xdr_traits<T>::from_uint(get64(p_)); }

  template<typename T> typename std::enable_if<xdr_traits<T>::is_bytes>::type
  operator()(T &t) {
    std::size_t size;
    if (xdr_traits<T>::variable_nelem) {
      check(4);
      size = get32(p_);
      check(size);
      t.resize(size);
    }
    else {
      size = t.size();
      check(size);
    }
    get_bytes(p_, t.data(), size);
  }

  template<typename T> typename std::enable_if<
    xdr_traits<T>::is_class || xdr_traits<T>::is_container>::type
  operator()(T &t) { xdr_traits<T>::load(*this, t); }
};

#if WORDS_BIGENDIAN
using xdr_put = xdr_generic_put<marshal_noswap>;
using xdr_get = xdr_generic_get<marshal_noswap>;
#else // !WORDS_BIGENDIAN
using xdr_put = xdr_generic_put<marshal_swap>;
using xdr_get = xdr_generic_get<marshal_swap>;
#endif // !WORDS_BIGENDIAN

inline std::size_t
xdr_argpack_size()
{
  return 0;
}
template<typename T, typename...Args> inline std::size_t
xdr_argpack_size(const T &t, const Args &...a)
{
  return xdr_size(t) + xdr_argpack_size(a...);
}

template<typename Archive> inline void
xdr_argpack_archive(Archive &)
{
}
template<typename Archive, typename T, typename...Args> inline void
xdr_argpack_archive(Archive &ar, T &&t, Args &&...args)
{
  archive(ar, std::forward<T>(t));
  xdr_argpack_archive(ar, std::forward<Args>(args)...);
}

template<typename...Args> msg_ptr
xdr_to_msg(const Args &...args)
{
  msg_ptr m (message_t::alloc(xdr_argpack_size(args...)));
  xdr_put p (m);
  xdr_argpack_archive(p, args...);
  assert(p.p_ == p.e_);
  return m;
}

/* XXX - Not really useful, since we have to unmarshal header then body */
template<typename T> T &
xdr_from_msg(const msg_ptr &m, T &t)
{
  xdr_get g(m);
  archive(g, t);
  if (g.p_ != g.e_)
    throw xdr_runtime_error("xdr_from_message did not consume whole message");
  return t;
}

}

#endif // !_XDRC_MARSHAL_H_HEADER_INCLUDED_

