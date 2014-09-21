// -*- C++ -*-

/** \file types.h Type definitions for xdrc compiler output. */

#ifndef _XDRC_TYPES_H_HEADER_INCLUDED_
#define _XDRC_TYPES_H_HEADER_INCLUDED_ 1

#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace xdr {

//! Generic class of XDR unmarshaling errors.
struct xdr_runtime_error : std::runtime_error {
  using std::runtime_error::runtime_error;
};

//! Attempt to exceed the bounds of a variable-length array or string.
struct xdr_overflow : xdr_runtime_error {
  using xdr_runtime_error::xdr_runtime_error;
};

//! Attempt to set invalid value for a union discriminant.
struct xdr_bad_value : xdr_runtime_error {
  using xdr_runtime_error::xdr_runtime_error;
};

//! Attempt to access wrong field of a union.
struct xdr_wrong_union : std::logic_error {
  using std::logic_error::logic_error;
};


//! \c value is \c true iff \c T is an XDR enum, in which case \c
//! xdr_enum also contains a static method \c name translating an
//! instance of the enum into a <tt>char *</tt> for pretty-printing.
template<typename T> struct xdr_enum : std::false_type {};

//! \c value is \c true iff \c T is an XDR struct or union type, in
//! which case \c xdr_class also contains static methods \c save and
//! \c load to traverse the fields of the structure.
template<typename T> struct xdr_class : std::false_type {};

//! \c value is true for fixed-length array, variable-length array, or
//! pointer.
template<typename T> struct xdr_container : std::false_type {};

constexpr std::uint32_t XDR_MAX_LEN = 0xffffffff;

//! XDR arrays are implemented using std::array
using std::array;

template<typename T, std::uint32_t N> struct xdr_container<array<T, N>>
  : std::true_type {
  using element_type = T;
  using container_type = array<T, N>;
  static constexpr bool variable = false;
  static constexpr bool pointer = false;
  static constexpr std::uint32_t size(const container_type &) { return N; }
  static T *begin(container_type &c) { return &*c.begin(); }
  static const T *begin(const container_type &c) { return &*c.cbegin(); }
  static const T *end(container_type &c) { &*c.cend(); }
};

//! Opaque is represented as std::uint8_t;
template<std::uint32_t N = XDR_MAX_LEN> using opaque_array =
  array<std::uint8_t, N>;

template<std::uint32_t N> struct xdr_container<opaque_array<N>>
  : std::false_type {};


//! A vector with a maximum size (returned by xvector::max_size()).
//! Note that you can exceed the size, but an error will happen when
//! marshaling or unmarshaling the data structure.
template<typename T, std::uint32_t N = XDR_MAX_LEN>
struct xvector : std::vector<T> {
  using vector = std::vector<T>;

  using vector::vector;

  //! Return the maximum size allowed by the type.
  static constexpr std::uint32_t max_size() { return N; }

  //! Check whether a size is in bounds
  static void check_size(size_t n) {
    if (n > max_size())
      throw xdr_overflow("xvector overflow");
  }
};

template<typename T, std::uint32_t N> struct xdr_container<xvector<T, N>>
  : std::true_type {
  using element_type = T;
  using container_type = xvector<T, N>;
  static constexpr bool variable = true;
  static constexpr bool pointer = false;
  static std::uint32_t size(const container_type &c) { return c.size(); }
  static T *begin(container_type &c) { return &*c.begin(); }
  static const T *begin(const container_type &c) { return &*c.cbegin(); }
  static const T *end(container_type &c) { &*c.cend(); }
};


//! Variable-length opaque data is just a vector of std::uint8_t.
template<std::uint32_t N = XDR_MAX_LEN> using opaque_vec =
  xvector<std::uint8_t, N>;

template<std::uint32_t N> struct xdr_container<opaque_vec<N>>
  : std::false_type {};


//! A string with a maximum length (returned by xstring::max_size()).
//! Note that you can exceed the size, but an error will happen when
//! marshaling or unmarshaling the data structure.
template<std::uint32_t N = XDR_MAX_LEN>
struct xstring : std::string {
  using string = std::string;

  //! Return the maximum size allowed by the type.
  static constexpr std::uint32_t max_size() { return N; }

  //! Check whether a size is in bounds
  static void check_size(size_t n) {
    if (n > max_size())
      throw xdr_overflow("xvector overflow");
  }

  //! Check that the string length is not greater than the maximum
  //! size.  \throws std::out_of_range and clears the contents of the
  //! string if it is too long.
  void validate() const { check_size(size()); }

  xstring() = default;
  xstring(const xstring &) = default;
  xstring(xstring &&) = default;
  xstring &operator=(const xstring &) = default;
  xstring &operator=(xstring &&) = default;

  template<typename...Args> xstring(Args&&...args)
    : string(std::forward<Args>(args)...) { validate(); }

  using string::data;
  char *data() { return &(*this)[0]; } // protobufs does this, so probably ok

//! \hideinitializer
#define ASSIGN_LIKE(method)					\
  template<typename...Args> xstring &method(Args&&...args) {	\
    string::method(std::forward<Args>(args)...);		\
    validate();							\
    return *this;						\
  }
  ASSIGN_LIKE(operator=)
  ASSIGN_LIKE(operator+=)
  ASSIGN_LIKE(append)
  ASSIGN_LIKE(push_back)
  ASSIGN_LIKE(assign)
  ASSIGN_LIKE(insert)
  ASSIGN_LIKE(replace)
  ASSIGN_LIKE(swap)
#undef ASSIGN_LIKE
};

//! Optional data (represented with pointer notation in XDR source).
template<typename T> using optional = std::unique_ptr<T>;

template<typename T> struct xdr_container<optional<T>> : std::true_type {
  using element_type = T;
  using container_type = optional<T>;
  static constexpr bool variable = true;
  static constexpr bool pointer = true;
  static std::uint32_t size(const container_type &c) { return c == true; }
  static T *begin(container_type &c) { return c ? c.get() : nullptr; }
  static const T *begin(const container_type &c) {
    return c ? c.get() : nullptr;
  }
  static const T *end(container_type &c) { return c ? begin() + 1 : begin(); }
};


struct case_constructor_t {
  constexpr case_constructor_t() {}
  void operator()() const {}
  template<typename T> void operator()(T *) const {}
  template<typename T, typename F> void operator()(T *t, F T::*f) const {
    new (static_cast<void *>(std::addressof(t->*f))) F{};
  }
};
constexpr case_constructor_t case_constructor;

struct case_destroyer_t {
  constexpr case_destroyer_t() {}
  void operator()() const {}
  template<typename T> void operator()(T *) const {}
  template<typename T, typename F> void operator()(T *t, F T::*f) const {
    (t->*f).~F();
  }
};
constexpr case_destroyer_t case_destroyer;

struct case_construct_from {
  void *dest_;
  constexpr case_construct_from(void *dest) : dest_(dest) {}
  void operator()() const {}
  template<typename T> void operator()(T &&) const {}
  template<typename T, typename F> void operator()(const T &t, F T::*f) const {
    new (static_cast<void *>(&(static_cast<T*>(dest_)->*f))) F{t.*f};
  }
  template<typename T, typename F> void operator()(T &&t, F T::*f) const {
    new (static_cast<void *>(&(static_cast<T*>(dest_)->*f))) F{std::move(t.*f)};
  }
};

struct case_assign_from {
  void *dest_;
  constexpr case_assign_from(void *dest) : dest_(dest) {}
  void operator()() const {}
  template<typename T> void operator()(T &&) const {}
  template<typename T, typename F> void operator()(const T &t, F T::*f) const {
    static_cast<T*>(dest_)->*f = t.*f;
  }
  template<typename T, typename F> void operator()(T &&t, F T::*f) const {
    static_cast<T*>(dest_)->*f = std::move(t.*f);
  }
};

template<typename Archive> struct case_save {
  Archive &ar_;
  const char *name_;
  constexpr case_save(Archive &ar, const char *name) : ar_(ar), name_(name) {}
  void operator()() const {}
  template<typename T> void operator()(const T *) const {}
  template<typename T, typename F> void operator()(const T *t, F T::*f) const {
    ar_(name_, t->*f);
  }
};

template<typename Archive> struct case_load {
  Archive &ar_;
  const char *name_;
  constexpr case_load(Archive &ar, const char *name) : ar_(ar), name_(name) {}
  void operator()() const {}
  template<typename T> void operator()(T *) const {}
  template<typename T, typename F> void operator()(T *t, F T::*f) const {
    ar_(name_, t->*f);
  }
};

}

#endif // !_XDRC_TYPES_H_HEADER_INCLUDED_

