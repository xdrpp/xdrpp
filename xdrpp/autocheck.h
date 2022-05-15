// -*- C++ -*-

/** \file autocheck.h Support for using the [autocheck
  * framework](https://github.com/thejohnfreeman/autocheck/wiki) with
  * XDR data types.  If you include this file (which requires the
  * autocheck headers to be available), you can generate arbitrary
  * instances of XDR types for testing your application.  For example:
  * \code
  *   // g will generate arbitrary values of type my_xdr_type
  *   autocheck::generator<my_xdr_type> g;
  *   // For variable size objects (vectors, strings, linked lists),
  *   // size indicates how big to make them.  For numeric values,
  *   // size affects how big the number is likely to be.
  *   size_t size = 50;
  *   my_xdr_type arbitrary_value = g(object_size);
  *   fuzz_with(arbitrary_value);
  * \endcode
  */

#ifndef _XDRC_AUTOCHECK_H_HEADER_INCLUDED_
#define _XDRC_AUTOCHECK_H_HEADER_INCLUDED_ 1

#include <autocheck/autocheck.hpp>
#include <xdrpp/types.h>

namespace xdr {

#ifndef XDR_AUTOCHECK_FUZZY_STRINGS
// If defined to 0, changes the behavior of the library to generate
// strings with ASCII characters with small size parameters.  The
// default (1) generates strings with arbitrary bytes.
#define XDR_AUTOCHECK_FUZZY_STRINGS 1
#endif // !XDR_AUTOCHECK_FUZZY_STRINGS

namespace detail {

template<typename T> constexpr T
to_int8(uint8_t u)
{
  return std::bit_cast<T>(u);
}

}

struct generator_t {
  std::size_t size_;
  std::size_t levels_;  // Only decrease size after this many levels
  constexpr explicit generator_t(std::size_t size, std::size_t levels = 2)
    : size_(size), levels_(levels) {}

  // Handle char and uint8_t (which can legally be the same type for
  // some compilers), allowing variable_nelem cases to handle both
  // containers and bytes (string/opaque).
  template<typename T> requires (sizeof(T) == 1)
  void operator()(T &t) const {
#if XDR_AUTOCHECK_FUZZY_STRINGS
    t = detail::to_int8<T>(uint8_t(autocheck::generator<int>{}(0x10000)));
#else // !XDR_AUTOCHECK_FUZZY_STRINGS
    t = detail::to_int8<T>(autocheck::generator<T>{}(size_));
#endif // !XDR_AUTOCHECK_FUZZY_STRINGS
  }

  template<xdr_numeric T>
  void operator()(T &t) const {
    t = autocheck::generator<T>{}(std::numeric_limits<T>::max());
  }

  template<xdr_enum T> requires (!std::same_as<T, bool>)
  void operator()(T &t) const {
    static_assert(!std::same_as<T, bool>);
    if(autocheck::generator<bool>{}(size_)) {
      typename xdr_traits<T>::case_type v;
      (*this)(v);
      t = T(v);
    }
    else {
      uint32_t n;
      (*this)(n);
      const auto &vals = xdr_traits<T>::enum_values();
      t = T(vals[n % vals.size()]);
    }
  }

  template<xdr_struct T>
  void operator()(T &t) const { xdr_traits<T>::load(*this, t); }

  template<xdr_union T>
  void operator()(T &t) const {
    const auto &vals = T::_xdr_case_values();
    typename xdr_traits<T>::discriminant_type v;
    if (!T::_xdr_has_default_case) {
      // Just pick a random case if there's no default
      uint32_t n;
      (*this)(n);
      v = vals[n % vals.size()];
    }
    else if (xdr_traits<decltype(v)>::is_enum)
      (*this)(v);
    else {
      if (autocheck::generator<bool>{}(size_))
	(*this)(v);
      else {
	uint32_t n;
	(*this)(n);
	v = vals[n % vals.size()];
      }
    }
    t._xdr_discriminant(v, false);
    t._xdr_with_body_accessor([this, &t](auto body) {
      archive(*this, body(t), nullptr);
    });
  }

  // Generator with shrunken size for elements of a container
  generator_t elt_gen() const {
    return levels_ ? generator_t(size_, levels_-1) : generator_t(size_>>1, 0);
  }

  template<typename T> requires xdr_traits<T>::variable_nelem
  void operator()(T &t) const {
    uint32_t n = autocheck::generator<uint32_t>{}(size_);
    if (n > t.max_size())
      n %= t.max_size() + 1;
    t.resize(n);
    generator_t g(elt_gen());
    for (auto &e : t)
      archive(g, e);
  }

  template<typename T> requires (!xdr_traits<T>::variable_nelem)
  void operator()(T &t) const {
    generator_t g(elt_gen());
    for (auto &e : t)
      archive(g, e);
  }

  template<typename T> void
  operator()(pointer<T> &t) const {
    if (autocheck::generator<std::uint32_t>{}(size_+1)) {
      generator_t g(elt_gen());
      archive(g, t.activate());
    }
    else
      t.reset();
  }
};

} // namespace xdr

namespace autocheck {

template<xdr::xdr_type T> requires (!xdr::xdr_numeric<T>)
class generator<T> {
public:
  using result_type = T;
  result_type operator()(size_t size) const {
    xdr::generator_t g(size);
    T t;
    xdr::archive(g, t);
    return t;
  }
};

} // namespace autocheck

#endif // !_XDRC_AUTOCHECK_H_HEADER_INCLUDED_
