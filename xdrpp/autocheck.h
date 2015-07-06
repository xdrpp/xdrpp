// -*- C++ -*-

/** \file autocheck.h Support for using the autocheck framework with
  * XDR data types. */

#ifndef _XDRC_AUTOCHECK_H_HEADER_INCLUDED_
#define _XDRC_AUTOCHECK_H_HEADER_INCLUDED_ 1

#include <autocheck/autocheck.hpp>
#include <xdrpp/types.h>

namespace xdr {

struct generator_t {
  std::size_t size_;  
  Constexpr generator_t(std::size_t size) : size_(size) {}

  template<typename T> typename
  std::enable_if<xdr_traits<T>::is_numeric>::type
  operator()(T &t) const { t = autocheck::generator<T>{}(size_); }

  template<typename T> typename
  std::enable_if<xdr_traits<T>::is_enum>::type
  operator()(T &t) const {
    t = xdr_traits<T>::from_uint(autocheck::generator<uint32_t>{}(size_));
  }

  template<typename T> typename
  std::enable_if<xdr_traits<T>::is_struct>::type
  operator()(T &t) const { xdr_traits<T>::load(*this, t); }

  static inline size_t elt_size(size_t nelts, size_t size)
  {
      switch (nelts)
      {
      case 0:
          return 0;
      case 1:
          return size >> 1;
      default:
          return size / (nelts >> 1);
      }
  }

  template<typename T> typename
  std::enable_if<xdr_traits<T>::is_union>::type
  operator()(T &t) const {
    const auto &vals = T::_xdr_discriminant_values();
    typename xdr_traits<T>::case_type v;
    size_t esize = elt_size(vals.size(), size_);
    if (vals.empty())
      v = autocheck::generator<decltype(v)>{}(esize);
    else {
      size_t n = autocheck::generator<size_t>{}(esize);
      v = vals[n % vals.size()];
    }
    t._xdr_discriminant(v, false);
    t._xdr_with_mem_ptr(field_archiver, v, *this, t, nullptr);
  }

  template<typename T> typename
  std::enable_if<xdr_traits<T>::variable_nelem == true>::type
  operator()(T &t) const {
    t.resize(autocheck::generator<std::uint32_t>{}(size_) % t.max_size());
    autocheck::generator<typename T::value_type> gen;
    size_t esize = elt_size(t.size(), size_);
    for (auto &e : t)
      e = gen(esize);
  }

  template<typename T> typename
  std::enable_if<xdr_traits<T>::variable_nelem == false>::type
  operator()(T &t) const {
    autocheck::generator<typename T::value_type> gen;
    size_t esize = elt_size(t.size(), size_);
    for (auto &e : t)
      e = gen(esize);
  }

  template<typename T> void
  operator()(pointer<T> &t) const {
    if (autocheck::generator<std::uint32_t>{}(size_)) {
      generator_t g(size_ >> 1);
      archive(g, t.activate());
    }
    else
      t.reset();
  }
};

} // namespace xdr

namespace autocheck {

template<typename T> class generator<
  T, typename std::enable_if<xdr::xdr_traits<T>::valid
			     && !xdr::xdr_traits<T>::is_numeric>::type> {
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

