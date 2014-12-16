// -*- C++ -*-

/** \file compare.h Binary comparison operators for xdrc-generated
 *  structs and unions.  You have to say <tt>using namespace
 *  xdr::eq</tt> to get \c operator==, or <tt>using namespace
 *  xdr::ord</tt> to get both \c operator== and \c operator<.  For ADL
 *  to work properly, these \c using declarations must be in the same
 *  namespace as the data structures you are comparing.
 */

#ifndef _XDRC_COMPARE_H_HEADER_INCLUDED_
#define _XDRC_COMPARE_H_HEADER_INCLUDED_ 1

#include <xdrpp/types.h>

namespace xdr {

namespace eq {

namespace detail {
struct union_field_equal_t {
  Constexpr union_field_equal_t() {}
  template<typename T, typename F>
  void operator()(F T::*mp, const T &a, const T &b, bool &out) const {
    out = a.*mp == b.*mp;
  }
};
Constexpr const union_field_equal_t union_field_equal {};
} // namespace detail

template<typename T> inline typename
std::enable_if<xdr_traits<T>::is_union, bool>::type
operator==(const T &a, const T &b)
{
  if (a._xdr_discriminant() == b._xdr_discriminant()) {
    bool r{true};
    a._xdr_with_mem_ptr(detail::union_field_equal,
			a._xdr_discriminant(), a, b, r);
    return r;
  }
  return false;
}

namespace detail {
template<typename T, typename F> struct struct_equal_helper {
  static bool equal(const T &a, const T &b) {
    Constexpr const typename F::field_info fi {};
    if (!(fi(a) == fi(b)))
      return false;
    return struct_equal_helper<T, typename F::next_field>::equal(a, b);
  }
};
template<typename T> struct struct_equal_helper<T, xdr_struct_base<>> {
  static bool equal(const T &, const T &) { return true; }
};
} // namespace detail

template<typename T> inline typename
std::enable_if<xdr_traits<T>::is_struct, bool>::type
operator==(const T &a, const T &b)
{
  return detail::struct_equal_helper<T, xdr_traits<T>>::equal(a, b);
}

} // namespace eq

namespace ord {

using namespace eq;

namespace detail {
struct union_field_lt_t {
  Constexpr union_field_lt_t() {}
  template<typename T, typename F>
  void operator()(F T::*mp, const T &a, const T &b, bool &out) const {
    out = a.*mp < b.*mp;
  }
};
Constexpr const union_field_lt_t union_field_lt {};
} // namespace detail

template<typename T> inline typename
std::enable_if<xdr_traits<T>::is_union, bool>::type
operator<(const T &a, const T &b)
{
  if (a._xdr_discriminant() == b._xdr_discriminant()) {
    bool r{true};
    a._xdr_with_mem_ptr(detail::union_field_lt,
			a._xdr_discriminant(), a, b, r);
    return r;
  }
  return a._xdr_discriminant() < b._xdr_discriminant();
}

namespace detail {
template<typename T, typename F> struct struct_lt_helper {
  static bool lt(const T &a, const T &b) {
    Constexpr const typename F::field_info fi {};
    if ((fi(a) == fi(b)))
      return struct_lt_helper<T, typename F::next_field>::lt(a, b);
    else
      return fi(a) < fi(b);
  }
};
template<typename T> struct struct_lt_helper<T, xdr_struct_base<>> {
  static bool lt(const T &, const T &) { return false; }
};
} // namespace detail

template<typename T> inline typename
std::enable_if<xdr_traits<T>::is_struct, bool>::type
operator<(const T &a, const T &b)
{
  return detail::struct_lt_helper<T, xdr_traits<T>>::lt(a, b);
}
} // namespace ord

} // namespace xdr


#endif // !_XDRC_COMPARE_H_HEADER_INCLUDED_
