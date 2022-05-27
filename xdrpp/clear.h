// -*- C++ -*-

/** \file clear.h Support for clearing an XDR data structure. */

#ifndef _XDRPP_CLEAR_H_HEADER_INCLUDED_
#define _XDRPP_CLEAR_H_HEADER_INCLUDED_ 1

#include <cstring>
#include <type_traits>
#include <xdrpp/types.h>

namespace xdr {

namespace detail{
//! Helper type for xdr::xdr_clear function.
struct xdr_clear_t {
  constexpr xdr_clear_t() {}

  template<xdr_type T> void operator()(T &t) const {
    if constexpr (xdr_numlike<T>)
      t = T{};
    else if constexpr (xdr_union<T>) {
      if (xdr_traits<T>::set_tag(t, typename xdr_traits<T>::tag_type{}))
	xdr_traits<T>::load(*this, t);
    }
    else if constexpr (requires { t.resize(0); })
      t.resize(0);
    else if constexpr (xdr_bytes<T>)
      memset(t.data(), 0, t.size());
    else
      xdr_traits<T>::load(*this, t);
  }
};
}

//! Reset XDR data structure to its default contents.  All vectors and
//! strings are set to length 0, all fixed-size opaque arrays zeroed
//! out, and all numeric and enum types sent to their default values
//! (generally 0).
template<typename T> void
xdr_clear(T &t)
{
  static constexpr detail::xdr_clear_t c;
  archive(c, t);
}

}

#endif // !_XDRPP_CLEAR_H_HEADER_INCLUDED
