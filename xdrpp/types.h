// -*- C++ -*-

/** \file types.h Type definitions for xdrc compiler output. */

#ifndef _XDRC_TYPES_H_HEADER_INCLUDED_
#define _XDRC_TYPES_H_HEADER_INCLUDED_ 1

#include <array>
#include <cassert>
#include <compare>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
#include <limits>

#include <xdrpp/endian.h>

//! Most of the xdrpp library is encapsulated in the xdr namespace.
namespace xdr {

using std::uint32_t;
using std::size_t;

inline uint32_t
size32(size_t s)
{
  uint32_t r {uint32_t(s)};
  assert(s == r);
  return r;
}


////////////////////////////////////////////////////////////////
// Exception types
////////////////////////////////////////////////////////////////

//! Generic class of XDR unmarshaling errors.
struct xdr_runtime_error : std::runtime_error {
  using std::runtime_error::runtime_error;
};

//! Attempt to exceed the bounds of a variable-length array or string.
struct xdr_overflow : xdr_runtime_error {
  using xdr_runtime_error::xdr_runtime_error;
};

//! Attempt to exceed recursion limits.
struct xdr_stack_overflow : xdr_runtime_error {
  using xdr_runtime_error::xdr_runtime_error;
};

//! Message not multiple of 4 bytes, or cannot fully be parsed.
struct xdr_bad_message_size : xdr_runtime_error {
  using xdr_runtime_error::xdr_runtime_error;
};

//! Attempt to set invalid value for a union discriminant.
struct xdr_bad_discriminant : xdr_runtime_error {
  using xdr_runtime_error::xdr_runtime_error;
};

//! Padding bytes that should have contained zero don't.
struct xdr_should_be_zero : xdr_runtime_error {
  using xdr_runtime_error::xdr_runtime_error;
};

//! Exception for use by \c xdr::xdr_validate.
struct xdr_invariant_failed : xdr_runtime_error {
  using xdr_runtime_error::xdr_runtime_error;
};

//! Attempt to access wrong field of a union.  Note that this is not
//! an \c xdr_runtime_error, because it cannot result from
//! unmarshalling garbage arguments.  Rather it is a logic error in
//! application code that neglected to check the union discriminant
//! before accessing the wrong field.
struct xdr_wrong_union : std::logic_error {
  using std::logic_error::logic_error;
};


////////////////////////////////////////////////////////////////
// Templates for XDR traversal and processing
////////////////////////////////////////////////////////////////

namespace detail {
// If a class actually contains a method called validate, then the
// validate function might as well call it.  So we use the standard
// gross overload resolution hack that argument 0 will match a
// pointer-to-method argument before it will match a ... argument.
template<typename T> inline void
call_validate(const T &t, decltype(&T::validate))
{
  t.validate();
}

// Conventional wisdom holds that varargs don't inline, but, perhpas
// because we don't actually call va_start, both gcc and clang seem
// able to make this compile to nothing (with optimization).
template<typename T> inline void
call_validate(const T &, ...)
{
}
}

//! If this function template is specialized, it provides a means of
//! placing extra restrictions on XDR data structures (beyond those of
//! the XDR specification).  When a specialized \c xdr::validate
//! function detects a bad data argument, it should throw an exception
//! of type \c xdr::xdr_invariant_failed.  Note this mechanism only
//! works for user-defined XDR structs and unions.  It does not work
//! for enums, typedef aliases, or built-in types (int, hyper, string,
//! vectors, etc.).
template<typename T> inline void
validate(const T &t)
{
  detail::call_validate(t, 0);
}

//! This is used to apply an archive to a field.  It is designed as a
//! template class that can be specialized to various archive formats,
//! since some formats may want the fied name and others not.  Other
//! uses include translating types to supertypes, e.g., so an archive
//! can handle \c std::string instead of \c xdr::xstring.  Never
//! invoke \c archive_adapter::apply directly.  Instead, call \c
//! xdr::archive, as the latter may be specialized for certain types.
template<typename Archive> struct archive_adapter {
  template<typename T> static void apply(Archive &ar, T &&t, const char *) {
    ar(std::forward<T>(t));
  }
};

//! By default, this function simply applies \c ar (which must be a
//! function object) to \c t.  However, it does so via the \c
//! xdr::archive_adapter template class, which can be specialized to
//! capture the field name as well.  Never specialize or overload this
//! function on \c Archive (specialize \c xdr::archive_adapter
//! instead).  However, in special cases (such as \c
//! xdr::transparent_ptr) it is reasonable to specialize this function
//! template on \c T.
template<typename Archive, typename T> inline void
archive(Archive &ar, T &&t, const char *name = nullptr)
{
  archive_adapter<Archive>::apply(ar, std::forward<T>(t), name);
}

//! Metadata for all marshalable XDR types.
template<typename T> struct xdr_traits {
  //! \c T is a valid XDR type that can be serialized.
  static constexpr const bool valid = false;
  //! \c T is defined by xdrpp/xdrc (as opposed to native or std types).
  static constexpr const bool xdr_defined = false;
  //! \c T is an \c xstring, \c opaque_array, or \c opaque_vec.
  static constexpr const bool is_bytes = false;
  //! \c T is an XDR struct.
  static constexpr const bool is_struct = false;
  //! \c T is an XDR union.
  static constexpr const bool is_union = false;
  //! \c T is an XDR struct or union.
  static constexpr const bool is_class = false;
  //! \c T is an XDR enum or bool (traits have enum_name).
  static constexpr const bool is_enum = false;
  //! \c T is an xdr::pointer, xdr::xarray, or xdr::xvector (with load/save).
  static constexpr const bool is_container = false;
  //! \c T is one of [u]int{32,64}_t, float, or double.
  static constexpr const bool is_numeric = false;
  //! \c T has a fixed size.
  static constexpr const bool has_fixed_size = false;
};

template<typename T> using xdr_get_traits =
  xdr_traits<std::remove_cv_t<std::remove_reference_t<T>>>;
#define XDR_GET_TRAITS(obj) xdr::xdr_get_traits<decltype(obj)>

#define XDR_CONCEPT(c) \
    template<typename T> concept xdr_##c = xdr_get_traits<T>::is_##c
XDR_CONCEPT(struct);
XDR_CONCEPT(union);
XDR_CONCEPT(class);
XDR_CONCEPT(enum);
XDR_CONCEPT(numeric);
XDR_CONCEPT(container);
XDR_CONCEPT(bytes);
#undef XDR_CONCEPT
// Any valid XDR type
template<typename T> concept xdr_type = xdr_get_traits<T>::valid;
template<typename T> concept xdr_numlike = xdr_numeric<T> || xdr_enum<T>;

namespace detail {
// When a type T includes itself recursively (for instance because it
// contains a vector of itself), xdr_traits<T> will be incomplete at
// some points where one needs to know if the structure has a fixed
// size.  However, such recursive structures don't have a fixed size,
// anyway, so it is okay to short-circuit and return a false
// has_fixed_size.  The point of has_fixed_size_t is to allow
// specializations (notably for xvector<T>) that short-cirtuit to
// false.
template<typename T> struct has_fixed_size_t
  : std::integral_constant<bool, xdr_traits<T>::has_fixed_size> {};
}

//! Return the marshaled size of an XDR data type.
template<typename T> size_t
xdr_size(const T&t)
{
  return xdr_traits<T>::serial_size(t);
}

//! Default xdr_traits values for actual XDR types, used as a
//! supertype for most xdr::xdr_traits specializations.
struct xdr_traits_base {
  static constexpr const bool valid = true;
  static constexpr const bool xdr_defined = true;
  static constexpr const bool is_bytes = false;
  static constexpr const bool is_class = false;
  static constexpr const bool is_enum = false;
  static constexpr const bool is_container = false;
  static constexpr const bool is_numeric = false;
  static constexpr const bool is_struct = false;
  static constexpr const bool is_union = false;
  static constexpr const bool has_fixed_size = false;
};


////////////////////////////////////////////////////////////////
// Support for numeric types and bool
////////////////////////////////////////////////////////////////

//! A reinterpret-cast like function that works between types such as
//! floating-point and integers of the same size.  Used in marshaling,
//! so that a single set of byteswap routines can be used on all
//! numeric types including floating point.  Uses a union to avoid
//! strict pointer aliasing problems.
template<typename To, typename From> inline To
xdr_reinterpret(From f)
{
  static_assert(sizeof(To) == sizeof(From),
		"xdr_reinterpret with different sized objects");
  union {
    From from;
    To to;
  };
  from = f;
  return to;
}

//! Default traits for use as supertype of specializations of \c
//! xdr_traits for integral types.
template<typename T, typename U> struct xdr_integral_base : xdr_traits_base {
  using type = T;
  using uint_type = U;
  static constexpr const bool xdr_defined = false;
  static constexpr const bool is_numeric = true;
  static constexpr const bool has_fixed_size = true;
  static constexpr const size_t fixed_size = sizeof(uint_type);
  static constexpr size_t serial_size(type) { return fixed_size; }
  static uint_type to_uint(type t) { return t; }
  static type from_uint(uint_type u) {
    return xdr_reinterpret<type>(u);
  }
};
template<> struct xdr_traits<std::int32_t>
  : xdr_integral_base<std::int32_t, std::uint32_t> {
  // Numeric type for case labels in switch statements
  using case_type = std::int32_t;
};
template<> struct xdr_traits<std::uint32_t>
  : xdr_integral_base<std::uint32_t, std::uint32_t> {
  using case_type = std::uint32_t;
};
template<> struct xdr_traits<std::int64_t>
  : xdr_integral_base<std::int64_t, std::uint64_t> {};
template<> struct xdr_traits<std::uint64_t>
  : xdr_integral_base<std::uint64_t, std::uint64_t> {};

//! Default traits for use as supertype of specializations of \c
//! xdr_traits for floating-point types.
template<typename T, typename U> struct xdr_fp_base : xdr_traits_base {
  using type = T;
  using uint_type = U;
  static_assert(sizeof(type) == sizeof(uint_type),
		"Cannot reinterpret between float and int of different size");
  static constexpr const bool xdr_defined = false;
  static constexpr const bool is_numeric = true;
  static constexpr const bool has_fixed_size = true;
  static constexpr const size_t fixed_size = sizeof(uint_type);
  static constexpr size_t serial_size(type) { return fixed_size; }

  static uint_type to_uint(type t) { return xdr_reinterpret<uint_type>(t); }
  static type from_uint(uint_type u) { return xdr_reinterpret<type>(u); }
};
template<> struct xdr_traits<float>
  : xdr_fp_base<float, std::uint32_t> {};
template<> struct xdr_traits<double>
  : xdr_fp_base<double, std::uint64_t> {};


template<> struct xdr_traits<bool>
  : xdr_integral_base<bool, std::uint32_t> {
  using case_type = std::int32_t;
  static constexpr const bool xdr_defined = false;
  static constexpr const bool is_enum = true;
  static constexpr const bool is_numeric = false;
  static type from_uint(uint_type u) { return u != 0; }
  static constexpr const char *enum_name(uint32_t b) {
    return b == 0 ? "FALSE" : b == 1 ? "TRUE" : nullptr;
  }
  static const std::vector<int32_t> &enum_values() {
    static const std::vector<int32_t> v = { 0, 1 };
    return v;
  }
};


////////////////////////////////////////////////////////////////
// XDR containers (xvector, xarrray, pointer) and bytes (xstring,
// opaque_vec, opaque_array)
////////////////////////////////////////////////////////////////

//! Maximum length of vectors.  (The RFC says 0xffffffff, but out of
//! paranoia for integer overflows we chose something that still fits
//! in 32 bits when rounded up to a multiple of four.)
static constexpr const uint32_t XDR_MAX_LEN = 0xfffffffc;

namespace detail {
//! Convenience supertype for traits of the three container types
//! (xarray, xvectors, and pointer).
template<typename T, bool variable,
	 bool VFixed = detail::has_fixed_size_t<typename T::value_type>::value>
struct xdr_container_base : xdr_traits_base {
  using value_type = typename T::value_type;
  static constexpr const bool is_container = true;
  //! Container has variable number of elements
  static constexpr const bool variable_nelem = variable;
  static constexpr const bool has_fixed_size = false;

  template<typename Archive> static void save(Archive &a, const T &t) {
    if (variable)
      archive(a, size32(t.size()));
    for (const value_type &v : t)
      archive(a, v);
  }
  template<typename Archive> static void load(Archive &a, T &t) {
    uint32_t n;
    if (variable) {
      archive(a, n);
      t.check_size(n);
      if (t.size() > n)
	t.resize(n);
    }
    else
      n = size32(t.size());
    for (uint32_t i = 0; i < n; ++i)
      archive(a, t.extend_at(i));
  }
  static size_t serial_size(const T &t) {
    size_t s = variable ? 4 : 0;
    for (const value_type &v : t)
      s += xdr_traits<value_type>::serial_size(v);
    return s;
  }
};

template<typename T> struct xdr_container_base<T, true, true>
  : xdr_container_base<T, true, false> {
  static size_t serial_size(const T &t) {
    return 4 + t.size() * xdr_traits<typename T::value_type>::fixed_size;
  }
};

template<typename T> struct xdr_container_base<T, false, true>
  : xdr_container_base<T, false, false> {
  static constexpr const bool has_fixed_size = true;
  static constexpr const size_t fixed_size =
    T::container_fixed_nelem * xdr_traits<typename T::value_type>::fixed_size;
  static size_t serial_size(const T &) { return fixed_size; }
};

//! Placeholder type to avoid clearing array
struct no_clear_t {
  constexpr no_clear_t() {}
};
constexpr const no_clear_t no_clear;
} // namespace detail

//! XDR arrays are implemented using std::array as a supertype.
template<typename T, uint32_t N> struct xarray
  : std::array<T, size_t(N)> {
  using array = std::array<T, size_t(N)>;
  xarray() { array::fill(T{}); }
  xarray(detail::no_clear_t) {}
  xarray(const xarray &) = default;
  xarray &operator=(const xarray &) = default;

  static constexpr const size_t container_fixed_nelem = N;
  static constexpr size_t size() { return N; }
  static void validate() {}
  static void check_size(uint32_t i) {
    if (i != N)
      throw xdr_overflow("invalid size in xdr::xarray");
  }
  static void resize(uint32_t i) {
    if (i != N)
      throw xdr_overflow("invalid resize in xdr::xarray");
  }
  T &extend_at(uint32_t i) {
    if (i >= N)
      throw xdr_overflow("attempt to access invalid position in xdr::xarray");
    return (*this)[i];
  }
};

template<typename T, uint32_t N>
struct xdr_traits<xarray<T,N>>
  : detail::xdr_container_base<xarray<T,N>, false> {};

//! XDR \c opaque is represented as std::uint8_t;
template<uint32_t N = XDR_MAX_LEN> struct opaque_array
  : xarray<std::uint8_t,N> {
  using xarray = xdr::xarray<std::uint8_t,N>;
  using xarray::xarray;
  // Pay a little performance to avoid heartbleed-type errors...
  opaque_array() : xarray(detail::no_clear) { std::memset(this->data(), 0, N); }
  opaque_array(detail::no_clear_t) : xarray(detail::no_clear) {}
};
template<uint32_t N> struct xdr_traits<opaque_array<N>> : xdr_traits_base {
  static constexpr const bool is_bytes = true;
  static constexpr const size_t has_fixed_size = true;
  static constexpr const size_t fixed_size =
    (size_t(N) + size_t(3)) & ~size_t(3);
  static size_t serial_size(const opaque_array<N> &) { return fixed_size; }
  static constexpr const bool variable_nelem = false;
};


//! A vector with a maximum size (returned by xvector::max_size()).
//! Note that you can exceed the size, but an error will happen when
//! marshaling or unmarshaling the data structure.
template<typename T, uint32_t N = XDR_MAX_LEN>
struct xvector : std::vector<T> {
  using vector = std::vector<T>;
  using vector::vector;

  //! Return the maximum size allowed by the type.
  static constexpr uint32_t max_size() { return N; }

  //! Check whether a size is in bounds
  static void check_size(size_t n) {
    if (n > max_size())
      throw xdr_overflow("xvector overflow");
  }

  void append(const T *elems, size_t n) {
    check_size(this->size() + n);
    this->insert(this->end(), elems, elems + n);
  }
  T &extend_at(uint32_t i) {
    if (i >= N)
      throw xdr_overflow("attempt to access invalid position in xdr::xvector");
    if (i == this->size())
      this->emplace_back();
    return (*this)[i];
  }
  void resize(uint32_t n) {
    check_size(n);
    vector::resize(n);
  }

  friend bool operator==(const xvector &a, const xvector &b) {
    return static_cast<const vector &>(a) == static_cast<const vector &>(b);
  }
  friend std::partial_ordering operator<=>(const xvector &a, const xvector &b) {
    return static_cast<const vector &>(a) <=> static_cast<const vector &>(b);
  }
};

namespace detail {
template<typename T> struct has_fixed_size_t<xvector<T>> : std::false_type {};
}

template<typename T, uint32_t N> struct xdr_traits<xvector<T,N>>
  : detail::xdr_container_base<xvector<T,N>, true> {};

//! Variable-length opaque data is just a vector of std::uint8_t.
template<uint32_t N = XDR_MAX_LEN> using opaque_vec = xvector<std::uint8_t, N>;
template<uint32_t N>
struct xdr_traits<xvector<std::uint8_t, N>> : xdr_traits_base {
  static constexpr const bool is_bytes = true;
  static constexpr const bool has_fixed_size = false;;
  static constexpr size_t serial_size(const opaque_vec<N> &a) {
    return (size_t(a.size()) + size_t(7)) & ~size_t(3);
  }
  static constexpr const bool variable_nelem = true;
};


//! A string with a maximum length (returned by xstring::max_size()).
//! Note that you can exceed the size, but an error will happen when
//! marshaling or unmarshaling the data structure.
template<uint32_t N = XDR_MAX_LEN> struct xstring : std::string {
  using string = std::string;

  //! Return the maximum size allowed by the type.
  static constexpr uint32_t max_size() { return N; }

  //! Check whether a size is in bounds
  static void check_size(size_t n) {
    if (n > max_size())
      throw xdr_overflow("xstring overflow");
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

  void resize(size_type n) { check_size(n); string::resize(n); }
  void resize(size_type n, char ch) { check_size(n); string::resize(n, ch); }
};

template<uint32_t N> struct xdr_traits<xstring<N>> : xdr_traits_base {
  static constexpr const bool is_bytes = true;
  static constexpr const bool has_fixed_size = false;;
  static constexpr size_t serial_size(const xstring<N> &a) {
    return (size_t(a.size()) + size_t(7)) & ~size_t(3);
  }
  static constexpr const bool variable_nelem = true;
};


//! Optional data (represented with pointer notation in XDR source).
template<typename T> struct pointer : std::unique_ptr<T> {
  using value_type = T;
  using std::unique_ptr<T>::unique_ptr;
  using std::unique_ptr<T>::get;
  pointer() = default;
  pointer(const pointer &p) : std::unique_ptr<T>(p ? new T(*p) : nullptr) {}
  pointer(pointer &&p) = default;
  pointer &operator=(const pointer &up) {
    if (const T *tp = up.get()) {
      if (T *selfp = this->get())
	*selfp = *tp;
      else
	this->reset(new T(*tp));
    }
    else
      this->reset();
    return *this;
  }
  pointer &operator=(pointer &&) = default;

  static void check_size(uint32_t n) {
    if (n > 1)
      throw xdr_overflow("xdr::pointer size must be 0 or 1");
  }
  uint32_t size() const { return *this ? 1 : 0; }
  T *begin() { return get(); }
  const T *begin() const { return get(); }
  T *end() { return begin() + size(); }
  const T *end() const { return begin() + size(); }
  T &extend_at(uint32_t i) {
    if (i != 0)
      throw xdr_overflow("attempt to access position > 0 in xdr::pointer");
    if (!size())
      this->reset(new T);
    return **this;
  }
  void resize(uint32_t n) {
    if (n == size())
      return;
    switch(n) {
    case 0:
      this->reset();
      break;
    case 1:
      this->reset(new T);
      break;
    default:
      throw xdr_overflow("xdr::pointer::resize: valid sizes are 0 and 1");
    }
  }
  T &activate() {
    if (!*this)
      this->reset(new T{});
    return *this->get();
  }

  //! Compare by value, rather than looking at the value of the pointer.
  friend bool operator==(const pointer &a, const pointer &b) {
    return (!a && !b) || (a && b && *a == *b);
  }
  friend std::partial_ordering operator<=>(const pointer &a, const pointer &b) {
    if (!a)
      return !b ? std::strong_ordering::equal : std::strong_ordering::less;
    if (!b)
      return std::strong_ordering::greater;
    return *a <=> *b;
  }
};

// Note an explicit third template argument (VFixed = false) is
// required because pointers are used recursively, so we might not
// have xdr_traits<T> available at the time we instantiate
// xdr_traits<pointer<T>>.
template<typename T> struct xdr_traits<pointer<T>>
  : detail::xdr_container_base<pointer<T>, true, false> {};


////////////////////////////////////////////////////////////////
// Support for XDR struct types
////////////////////////////////////////////////////////////////

//! Type-level representation of a pointer-to-member value.  When used
//! as a function object, dereferences the field, and returns it as
//! the same reference type as its argument (lvalue rference, const
//! lvalue reference, or const rvalue reference).
template<typename T, typename F, F T::*Ptr> struct field_ptr {
  using class_type = T;
  using field_type = F;
  using value_type = F T::*;
  //static constexpr value_type value = Ptr;
  static constexpr value_type value() { return Ptr; }
  F &operator()(T &t) const { return t.*Ptr; }
  const F &operator()(const T &t) const { return t.*Ptr; }
  F &operator()(T &&t) const { return std::move(t.*Ptr); }
};

template<typename ...Fields> struct xdr_struct_base;

namespace detail {
//! Default traits for fixed-size structures.
template<typename FP, typename ...Fields>
  struct xdr_struct_base_fs : xdr_struct_base<Fields...> {
  static constexpr const bool has_fixed_size = true;
  static constexpr const size_t fixed_size =
    (xdr_traits<typename FP::field_type>::fixed_size
     + xdr_struct_base<Fields...>::fixed_size);
  static constexpr size_t serial_size(const typename FP::class_type &) {
    return fixed_size;
  }
};
//! Default traits for variable-size structures.
template<typename FP, typename ...Fields>
  struct xdr_struct_base_vs : xdr_struct_base<Fields...> {
  static constexpr const bool has_fixed_size = false;
  static size_t serial_size(const typename FP::class_type &t) {
    return (xdr_size(t.*(FP::value()))
	    + xdr_struct_base<Fields...>::serial_size(t));
  }
};
}

//! Supertype to construct XDR traits of structure objects, used in
//! output of the \c xdrc compiler.
template<> struct xdr_struct_base<> : xdr_traits_base {
  static constexpr const bool is_class = true;
  static constexpr const bool is_struct = true;
  static constexpr const bool has_fixed_size = true;
  static constexpr const size_t fixed_size = 0;
  template<typename T> static constexpr size_t serial_size(const T&) {
    return fixed_size;
  }
};
template<typename FP, typename ...Rest> struct xdr_struct_base<FP, Rest...>
  : std::conditional<(detail::has_fixed_size_t<typename FP::field_type>::value
		      && xdr_struct_base<Rest...>::has_fixed_size),
    detail::xdr_struct_base_fs<FP, Rest...>,
    detail::xdr_struct_base_vs<FP, Rest...>>::type {
  using field_info = FP;
  using next_field = xdr_struct_base<Rest...>;
};


////////////////////////////////////////////////////////////////
// XDR-compatible representations of std::tuple and xdr_void
////////////////////////////////////////////////////////////////

//! Placehoder type representing void values marshaled as 0 bytes.
using xdr_void = std::tuple<>;

namespace detail {

template<typename> struct num_template_arguments;
template<template<typename...> typename Tmpl, typename...Args>
struct num_template_arguments<Tmpl<Args...>>
  : std::integral_constant<size_t, sizeof...(Args)> {};

template<typename T> constexpr auto
all_indices_of(const T&)
{
  if constexpr(requires { std::tuple_size_v<T>; })
    return std::make_index_sequence<std::tuple_size_v<T>>{};
  else
    return std::make_index_sequence<num_template_arguments<T>::value>{};
}

} // namespace detail

//! Invoke a function object \c f with a series of \c
//! std::integral_constant<size_t> values from 0 to one less than the
//! number of type arguments to the template type that is the first
//! argument.  For example, if the first argument is a \c std::tuple,
//! then the arguments can be taken as a parameter pack \c auto...i
//! and passed to \c std::get, as in the following example:
//!
//! \code
//!    std::tuple t("hello", " world ", 5, "\n");
//!    with_indices(t, [&t](auto...i) {
//!      (std::cout << ... << std::get<i>(t));
//!    });
//! \endcode
template<typename F, typename T> constexpr decltype(auto)
with_indices(const T &t, F &&f)
{
  return [&f]<std::size_t...Is>(std::index_sequence<Is...>) constexpr
    -> decltype(auto) {
    return std::forward<F>(f)(std::integral_constant<std::size_t, Is>{}...);
  }(detail::all_indices_of(t));
}

//! Invoce function object \c f once per index of a tuple or similar
//! type, passing it a \c std::integral_constant representing each
//! index.  Example:
//!
//! \code
//!    std::tuple t("hello", " world ", 5, "\n");
//!    for_each_index(t, [&t](auto i) { std::cout << std::get<i>(t); });
//! \endcode
constexpr void
for_each_index(const auto &t, auto &&f)
{
  with_indices(t, [&f](auto...i) constexpr { (void(f(i)), ...); });
}

namespace detail {

template<char...Cs> constexpr char bracketed_string[] = {'<', Cs..., '>', '\0'};

template<size_t N, char...Cs>
constexpr const char *
index_string(std::integral_constant<size_t, N> = {})
{
  if constexpr (N < 10)
    return bracketed_string<N+'0', Cs...>;
  else
    return index_string<N/10, (N%10)+'0', Cs...>();
}

template<typename...Ts>
constexpr bool all_fixed_size = (xdr_traits<Ts>::has_fixed_size && ...);

template<bool Fixed, typename...Ts> struct xdr_tuple_base_traits;

template<typename...Ts>
struct xdr_tuple_base_traits<true, Ts...>
  : xdr_traits_base {
  using type = std::tuple<Ts...>;
  static constexpr const bool xdr_defined = false;
  static constexpr const bool is_class = true;
  static constexpr const bool is_struct = true;
  static constexpr bool has_fixed_size = true;
  static constexpr std::uint32_t fixed_size =
    (xdr_traits<Ts>::fixed_size + ... + 0);
  static constexpr size_t serial_size(const type &) {
    return fixed_size;
  }
};

template<typename...Ts>
struct xdr_tuple_base_traits<false, Ts...>
  : xdr_traits_base {
  using type = std::tuple<Ts...>;
  static constexpr const bool xdr_defined = false;
  static constexpr const bool is_class = true;
  static constexpr const bool is_struct = true;
  static constexpr bool has_fixed_size = false;
  static size_t serial_size(const type &t) {
    return with_indices(t, [&t](auto...i) {
      return (0 + ... + xdr_traits<Ts>::serial_size(std::get<i>(t)));
    });
  }
};

} // namespace detail

template<typename...Ts>
struct xdr_traits<std::tuple<Ts...>>
  : public detail::xdr_tuple_base_traits<detail::all_fixed_size<Ts...>, Ts...>
{
  using type = std::tuple<Ts...>;
  static constexpr bool xdr_defined = false;
  static constexpr bool is_class = true;
  static constexpr bool is_struct = true;

  template<typename Archive> static void save(Archive &a, const type &t) {
    for_each_index(t, [&a,&t](auto i) {
      archive(a, std::get<i>(t), detail::index_string(i));
    });
  }
  template<typename Archive> static void load(Archive &a, type &t) {
    for_each_index(t, [&a,&t](auto i) {
      archive(a, std::get<i>(t), detail::index_string(i));
    });
  }
};


////////////////////////////////////////////////////////////////
// XDR union types
////////////////////////////////////////////////////////////////

namespace detail {

template<typename S, auto Field>
concept accepts_field_ptr = requires(S s) { s.*Field; };

} // namespace detail

template<auto Field> struct field_accessor_t;
template<typename S, typename F, F S::*Field>
struct field_accessor_t<Field> {
  constexpr field_accessor_t() {}

  template<detail::accepts_field_ptr<Field> T>
  decltype(auto) operator()(T &&t) const {
    return std::forward<T>(t).*Field;
  }
};
template<auto Field> constexpr field_accessor_t<Field> field_accessor{};

template<typename F, auto Field> struct uptr_accessor_t;
template<typename F, typename S, void *S::*Field>
struct uptr_accessor_t<F, Field> {
  constexpr uptr_accessor_t() {}

  using field_type = F;

  const F &operator()(const S &s) const {
    return *static_cast<const F*>(s.*Field);
  }
  F &operator()(S &s) const {
    return *static_cast<F*>(s.*Field);
  }
  F &&operator()(S &&s) const {
    return std::move(*static_cast<F*>(s.*Field));
  }
};
template<typename F, auto Field>
constexpr uptr_accessor_t<F, Field> uptr_accessor{};

} // namespace xdr

#endif // !_XDRC_TYPES_H_HEADER_INCLUDED_
