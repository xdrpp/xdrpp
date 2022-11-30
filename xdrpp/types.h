// -*- C++ -*-

/** \file types.h Type definitions for xdrc compiler output. */

#ifndef _XDRC_TYPES_H_HEADER_INCLUDED_
#define _XDRC_TYPES_H_HEADER_INCLUDED_ 1

#include <array>
#include <bit>
#include <cassert>
#include <compare>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <functional>
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
  assert(std::cmp_less_equal(s, 0xffffffff));
  return s;
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
  if constexpr (requires { t.validate(); })
    t.validate();
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
  static constexpr bool valid = false;
  //! \c T is defined by xdrpp/xdrc (as opposed to native or std types).
  static constexpr bool xdr_defined = false;
  //! \c T is an \c xstring, \c opaque_array, or \c opaque_vec.
  static constexpr bool is_bytes = false;
  //! \c T is an XDR struct.
  static constexpr bool is_struct = false;
  //! \c T is an XDR union.
  static constexpr bool is_union = false;
  //! \c T is an XDR struct or union.
  static constexpr bool is_class = false;
  //! \c T is an XDR enum or bool (traits have enum_name).
  static constexpr bool is_enum = false;
  //! \c T is an xdr::pointer, xdr::xarray, or xdr::xvector (with load/save).
  static constexpr bool is_container = false;
  //! \c T is one of [u]int{32,64}_t, float, or double.
  static constexpr bool is_numeric = false;
  //! \c T has a fixed size.
  static constexpr bool has_fixed_size = false;
};

template<typename T> using xdr_get_traits = xdr_traits<std::remove_cvref_t<T>>;
#define XDR_GET_TRAITS(obj) xdr::xdr_get_traits<decltype(obj)>

template<typename Src, typename Dst> concept can_construct =
  std::constructible_from<Dst, Src>;

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
template<xdr_type T> inline size_t
xdr_size(const T &t)
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

template<typename T, typename U> struct xdr_numeric_base : xdr_traits_base {
  using type = T;
  using uint_type = U;
  static_assert(sizeof(type) == sizeof(uint_type),
		"xdr_numeric_base size mismatch");
  static constexpr const bool xdr_defined = false;
  static constexpr const bool is_numeric = true;
  static constexpr const bool has_fixed_size = true;
  static constexpr const size_t fixed_size = sizeof(uint_type);
  static constexpr size_t serial_size(type) { return fixed_size; }
  static uint_type to_uint(type t) { return std::bit_cast<uint_type>(t); }
  static type from_uint(uint_type u) { return std::bit_cast<type>(u); }
};

template<> struct xdr_traits<std::int32_t>
  : xdr_numeric_base<std::int32_t, std::uint32_t> {
  // Numeric type for case labels in switch statements
  using case_type = std::int32_t;
};
template<> struct xdr_traits<std::uint32_t>
  : xdr_numeric_base<std::uint32_t, std::uint32_t> {
  using case_type = std::uint32_t;
};
template<> struct xdr_traits<std::int64_t>
  : xdr_numeric_base<std::int64_t, std::uint64_t> {};
template<> struct xdr_traits<std::uint64_t>
  : xdr_numeric_base<std::uint64_t, std::uint64_t> {};

template<> struct xdr_traits<float>
  : xdr_numeric_base<float, std::uint32_t> {};
template<> struct xdr_traits<double>
  : xdr_numeric_base<double, std::uint64_t> {};


template<> struct xdr_traits<bool> : xdr_traits_base {
  using type = bool;
  using uint_type = uint32_t;
  using case_type = std::int32_t; // So compilers don't warn about switch(bool)
  static constexpr const bool xdr_defined = false;
  static constexpr const bool is_enum = true;
  static constexpr const bool is_numeric = false;
  static constexpr const bool has_fixed_size = true;
  static constexpr const size_t fixed_size = sizeof(uint_type);
  static constexpr size_t serial_size(type) { return fixed_size; }
  static uint_type to_uint(bool b) { return b; }
  // Note - could throw for u > 1, but would be slower
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
  friend bool operator==(const pointer &a, const pointer &b) noexcept {
    return (!a && !b) || (a && b && *a == *b);
  }
  friend std::partial_ordering operator<=>(const pointer &a,
					   const pointer &b) noexcept {
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

namespace detail {

template<typename...Ts> concept all_have_fixed_size =
  requires { (0 + ... + xdr_traits<Ts>::fixed_size); };

template<bool Fixed, typename...Members> struct xdr_fixed_size_base_helper;
template<typename...Members>
struct xdr_fixed_size_base_helper<true, Members...> : xdr_traits_base {
  static constexpr bool has_fixed_size = true;
  static constexpr uint32_t fixed_size =
    (0 + ... + xdr_traits<Members>::fixed_size);
};
template<typename...Members>
struct xdr_fixed_size_base_helper<false, Members...> : xdr_traits_base {
  static constexpr bool has_fixed_size = false;
};

template<typename...Members> using xdr_fixed_size_base =
  xdr_fixed_size_base_helper<all_have_fixed_size<Members...>, Members...>;

//! Compile-time string suitable as a non-type template argument.
template<size_t N>
struct fixed_string {
  char value[N];
  consteval fixed_string(const char (&str)[N]) {
    std::copy_n(str, N, value);
  }
  /*
  template<size_t M> consteval
  bool operator==(fixed_string<M> s2) const {
    if constexpr (N != M)
      return false;
    else
      return std::equal(value, value+N, s2.value);
  }
  */
};

} // namespace detail

template<auto F, detail::fixed_string>
requires std::is_member_object_pointer_v<decltype(F)>
struct field_access_t;

template<typename S, typename T, T S::*F, detail::fixed_string Name>
struct field_access_t<F, Name> {
  using struct_type = S;
  using field_type = T;
  using value_type = decltype(F);
  static constexpr auto field_name = Name;
  static constexpr value_type value = F;
  static constexpr bool has_field = true;

  static constexpr const char *name() { return field_name.value; }

  constexpr decltype(auto) operator()(struct_type &s) const {
    return s.*value;
  }
  constexpr decltype(auto) operator()(const struct_type &s) const {
    return s.*value;
  }
  constexpr decltype(auto) operator()(struct_type &&s) const {
    return std::move(s).*value;
  }
};

namespace detail {

template<typename T> struct struct_meta;

template<typename T>
requires requires { typename T::_xdr_struct_meta; }
struct struct_meta<T> {
  using type = typename T::_xdr_struct_meta;
};

template<typename T, typename F>
requires requires { typename struct_meta<T>::type; }
inline decltype(auto)
with_struct_fields(F &&f)
{
  using meta = typename struct_meta<T>::type;
  return [&f]<size_t ...I>(std::index_sequence<I...>) -> decltype(auto) {
    return std::forward<F>(f)(meta::template field_access<I>()...);
  }(std::make_index_sequence<meta::num_fields>{});
}
} // namespace detail

template<typename S>
requires requires { typename detail::struct_meta<S>::type; }
struct xdr_struct_base
  : decltype(detail::with_struct_fields<S>([]<typename...Fs>(Fs...) {
	return detail::xdr_fixed_size_base<typename Fs::struct_type...>{};
      })) {
  using struct_type = S;
  static constexpr const bool is_class = true;
  static constexpr const bool is_struct = true;

  static constexpr size_t serial_size(const struct_type &obj) {
    if constexpr (xdr_struct_base::has_fixed_size)
      return xdr_struct_base::fixed_size;
    else
      return detail::with_struct_fields<struct_type>
	([&obj]<typename...Fs>(Fs...fs) {
	  return (0 + ... + xdr_get_traits<typename Fs::field_type>::
		  serial_size(fs(obj)));
	});
  }

  template<typename Archive> static void
  load(Archive &ar, struct_type &obj) {
    detail::with_struct_fields<struct_type>([&ar,&obj](auto ...fs) {
      (void(archive(ar, fs(obj), fs.name())), ...);
    });
    validate(obj);
  }
  template<typename Archive> static void
  save(Archive &ar, const struct_type &obj) {
    detail::with_struct_fields<struct_type>([&ar,&obj](auto ...fs) {
      (void(archive(ar, fs(obj), fs.name())), ...);
    });
  }
};


////////////////////////////////////////////////////////////////
// XDR-compatible representations of std::tuple and xdr_void
////////////////////////////////////////////////////////////////

//! Placehoder type representing void values marshaled as 0 bytes.
using xdr_void = std::tuple<>;

namespace detail {

template<char...Cs> constexpr char bracketed_string[] = {'<', Cs..., '>', '\0'};

template<size_t N, char...Cs>
consteval const char *
index_string(std::integral_constant<size_t, N> = {})
{
  if constexpr (N < 10)
    return bracketed_string<N+'0', Cs...>;
  else
    return index_string<N/10, (N%10)+'0', Cs...>();
}

template<typename T, size_t I>
struct tuple_access {
  using struct_type = T;
  static constexpr size_t value = I;
  using field_type = std::tuple_element_t<value, struct_type>;

  static constexpr const char *name() { return index_string<value>(); }

  constexpr decltype(auto) operator()(struct_type &s) const {
    return get<value>(s);
  }
  constexpr decltype(auto) operator()(const struct_type &s) const {
    return get<value>(s);
  }
  constexpr decltype(auto) operator()(struct_type &&s) const {
    return get<value>(std::move(s));
  }
};

template<typename...T>
struct struct_meta<std::tuple<T...>> {
  struct type {
    static constexpr size_t num_fields = sizeof...(T);

    template<size_t I> requires (I < num_fields)
    static constexpr auto field_access () {
      return tuple_access<std::tuple<T...>, I>{};
    }
  };
};
} // namespace detail

template<typename ...Ts>
struct xdr_traits<std::tuple<Ts...>>
  : xdr_struct_base<std::tuple<Ts...>> {
  static constexpr bool xdr_defined = false;
};


////////////////////////////////////////////////////////////////
// XDR union types
////////////////////////////////////////////////////////////////

// Placeholder dummy type for void union branches (where non-void
// branches take a field_access_t).
struct void_access_t {
  using field_type = void;
  static constexpr bool has_field = false;
  static constexpr const char *name() { return ""; }
  xdr_void &operator()(auto &&) const {
    static xdr_void dummy_void{};
    return dummy_void;
  }
};

template<typename T, auto Field, detail::fixed_string Name>
struct uptr_access_t;
template<typename T, typename S, void *S::*Field, detail::fixed_string Name>
struct uptr_access_t<T, Field, Name> {
  using struct_type = S;
  using field_type = T;
  using value_type = decltype(Field);
  static constexpr auto field_name = Name;
  static constexpr value_type value = Field;

  static constexpr const char *name() { return field_name.value; }

  decltype(auto) operator()(const struct_type &s) const {
    return *static_cast<const field_type*>(s.*Field);
  }
  decltype(auto) operator()(struct_type &s) const {
    return *static_cast<field_type*>(s.*Field);
  }
  decltype(auto) operator()(struct_type &&s) const {
    return std::move(*static_cast<field_type*>(s.*Field));
  }
};

namespace detail {

template<xdr_enum E> inline std::string
show_tag(E e)
{
  return xdr_get_traits<E>::enum_name(e);
}

template<xdr_numeric N> inline std::string
show_tag(N n)
{
  return std::to_string(n);
}

// helper function for with_n
template<std::size_t I, typename R, typename F>
constexpr R
with_integral_constant(F f)
{
  return std::forward<F>(f)(std::integral_constant<std::size_t, I>{});
}

} // detail

// Invoke an object with a compile-time constant corresponding to the
// runtime value of n.  Template parameter N is the bound on n, which
// must reside in the half-open range [0,N).
// See here for a detailed explanation of the code:
// https://www.scs.stanford.edu/~dm/blog/param-pack.html#array-of-function-pointers
template<std::size_t N, typename R = void, typename F>
inline constexpr R
with_n(size_t n, F &&f)
{
  constexpr auto invoke_array = []<size_t...I>(std::index_sequence<I...>) {
      return std::array{ detail::with_integral_constant<I, R, F&&>... };
    }(std::make_index_sequence<N>{});

  return invoke_array.at(n)(std::forward<F>(f));
}

template<typename T> concept is_field_access =
  std::same_as<T, field_access_t<T::value, T::field_name>>;

template<typename T> concept is_uptr_access =
    std::same_as<T, uptr_access_t<typename T::field_type, T::value,
				  T::field_name>>;

// XDR union types with an _xdr_union_meta inner struct.
template<typename U> concept has_union_meta_strict =
  requires { typename U::_xdr_union_meta; };

// has_union_meta_strict or const/references to has_union_meta_strict
template<typename U> concept has_union_meta =
  requires { typename std::remove_cvref_t<U>::_xdr_union_meta; };

struct unionfn {
  template<typename U> using union_meta =
    typename std::remove_cvref_t<U>::_xdr_union_meta;
  template<typename U> using tag_type =
    typename decltype(union_meta<U>::tag_access())::field_type;

  template<has_union_meta U>
  static size_t fieldno(const U &u) {
    return union_meta<U>::fieldno(tagref(u));
  }

  template<has_union_meta U>
  static consteval const char *get_tag_name() {
    return union_meta<U>::tag_access().name();
  }

  template<has_union_meta U>
  static tag_type<U> get_tag(const U &u) {
    return tagref(u);
  }

  template<has_union_meta_strict U>
  static bool set_tag(U &u, tag_type<U> v) {
    const int oldno = fieldno(u), newno = union_meta<U>::fieldno(v);
    if (oldno == newno)
      tagref(u) = v;
    else {
      with_current_arm(u, [&u](auto f) { destroy_field(f, u); });
      tagref(u) = v;
      with_current_arm(u, [&u](auto f) { construct_field(f, u); });
    }
    return newno >= 0;
  }

  template<has_union_meta_strict U, typename R>
  requires std::same_as<U, std::remove_cvref_t<R>>
  static U &assign(U &u, R &&r) {
    if (fieldno(u) == fieldno(r)) {
      tagref(u) = tagref(r);
      with_current_arm(u, [&](auto f) { f(u) = f(std::forward<R>(r)); });
    }
    else {
      destructor(u);
      copy_constructor(u, std::forward<R>(r));
    }
    return u;
  }

  template<size_t I, typename U> requires (I < union_meta<U>::num_arms)
  static decltype(auto) arm_impl(U &&u) {
    if (fieldno(u) == I)
      return armref<I>(u);
    std::string errmsg = std::string("accessed field ") +
      union_meta<U>::template arm_access<I>().name() +
      " in " + union_meta<U>::union_name +
      " but " + union_meta<U>::tag_access().name()
      + " == " + detail::show_tag(tagref(u));
    throw xdr_wrong_union(errmsg);
  }

  template<has_union_meta U>
  static void check_tag(tag_type<U> t) {
    if constexpr (!union_meta<U>::has_default_case)
      if (union_meta<U>::fieldno(t) == -1) [[unlikely]] {
	std::string errmsg =
	  std::string("bad value ") + detail::show_tag(t) +
	  " for " + union_meta<U>::tag_access().name() +
	  " in " + union_meta<U>::union_name;
	throw xdr_bad_discriminant(errmsg);
      }
  }

  static void construct_field(void_access_t, auto&&, auto&&...) {}
  static void destroy_field(void_access_t, auto&&) {}

  template<is_uptr_access A, typename ...Args>
  static void construct_field(A f, typename A::struct_type &s, Args&&...args) {
    s.*f.value = new typename A::field_type(std::forward<Args>(args)...);
  }
  template<is_uptr_access A>
  static void destroy_field(A f, typename A::struct_type &s) {
    delete &f(s);
    s.*f.value = nullptr;
  }

  template<is_field_access A, typename...Args>
  static void construct_field(A f, typename A::struct_type &s, Args&&...args) {
    // XXX - could be awkward if constructor throws
    std::construct_at(&f(s), std::forward<Args>(args)...);
  }
  template<is_field_access A>
  static void destroy_field(A f, typename A::struct_type &s) {
    std::destroy_at(&f(s));
  }

  template<has_union_meta U, typename F>
  static void with_current_arm(U &&u, F &&f) {
    if (int n = fieldno(u); n > 0)
      return with_n<union_meta<U>::num_arms>(n, [&f](auto i) {
	std::forward<F>(f)(union_meta<U>::template arm_access<i>());
      });
  }

  template<has_union_meta_strict U, typename ...Args>
  static void constructor(U &u, Args&&...args) {
    with_current_arm(u, [&](auto f) {
      construct_field(f, u, std::forward<Args>(args)...);
    });
  }

  // Copy or move constructor
  template<has_union_meta_strict U, typename R>
  requires std::same_as<U, std::remove_cvref_t<R>>
  static void copy_constructor(U &u, R &&r) {
    with_current_arm(r, [&](auto f) {
      construct_field(f, u, f(std::forward<R>(r)));
    });
    tagref(u) = tagref(r);
  }

  template<has_union_meta_strict U>
  static void destructor(U &u) {
    with_current_arm(u, [&](auto f) { destroy_field(f, u); });
  }

private:
  template<has_union_meta U>
  static decltype(auto) tagref(U &&u) {
    return union_meta<U>::tag_access()(u);
  }

  template<size_t I, has_union_meta U>
  static decltype(auto) armref(U &&u) {
    return union_meta<U>::template arm_access<I>()(u);
  }
};


template<typename U>
struct xdr_union_base
  : public xdr_traits_base {
  using union_type = U;

  static constexpr bool is_union = true;
  static constexpr bool is_class = true;

  static size_t serial_size(const union_type &u) {
    size_t s = 4;
    unionfn::with_current_arm(u, [&](auto f) {
      s += XDR_GET_TRAITS(f(u))::serial_size(f(u));
    });
    return s;
  }

  template<typename Archive>
  static void save(Archive &ar, const union_type &u) {
    const auto t = unionfn::get_tag(u);
    unionfn::check_tag<union_type>(t);
    archive(ar, t, unionfn::get_tag_name<union_type>());
    unionfn::with_current_arm(u, [&](auto f) { archive(ar, f(u), f.name()); });
  }
  template<typename Archive>
  static void load(Archive &ar, union_type &u) {
    auto t = unionfn::get_tag(u);
    archive(ar, t, unionfn::get_tag_name<union_type>());
    unionfn::check_tag<union_type>(t);
    unionfn::set_tag(u, t);
    unionfn::with_current_arm(u, [&](auto f) { archive(ar, f(u), f.name()); });
  }
};


////////////////////////////////////////////////////////////////
// Comparison for structs and unions
////////////////////////////////////////////////////////////////

template<xdr_struct T> requires xdr_traits<T>::xdr_defined
constexpr bool
operator==(const T &a, const T &b) noexcept
{
  return detail::with_struct_fields<T>([&](auto ...f) {
    return ((f(a) == f(b)) && ...);
  });
}

namespace detail {

template<typename...T> using field_cmp_res_t =
  std::common_comparison_category_t<std::compare_three_way_result_t<T>...>;

} // namespace detail

template<xdr_struct T> requires xdr_traits<T>::xdr_defined constexpr
#if __clang__
// work around compiler bug
std::enable_if_t<xdr_traits<T>::is_struct, std::partial_ordering>
#else // not clang
auto
#endif // not clang
operator<=>(const T &a, const T &b) noexcept
{
  return detail::with_struct_fields<T>([&](auto ...f) {
    detail::field_cmp_res_t<typename decltype(f)::field_type...> r =
      std::strong_ordering::equal;
    return void(((r = f(a) <=> f(b), r == 0) && ...)), r;
  });
}

template<xdr_union T> inline bool
operator==(const T &a, const T &b) noexcept
{
  if (unionfn::get_tag(a) != unionfn::get_tag(b))
    return false;
  bool r = true;
  unionfn::with_current_arm(a, [&](auto body) {
    r = body(a) == body(b);
  });
  return r;
}

template<xdr_union T> inline
#if __clang__
std::enable_if_t<xdr_traits<T>::is_union, std::partial_ordering>
#else // not clang
std::partial_ordering
#endif // not clang
operator<=>(const T &a, const T &b) noexcept
{
  if (auto c = unionfn::get_tag(a) <=> unionfn::get_tag(b); c != 0)
    return c;
  auto r = std::partial_ordering::equivalent;
  unionfn::with_current_arm(a, [&](auto body) {
    r = body(a) <=> body(b);
  });
  return r;
}

} // namespace xdr

#endif // !_XDRC_TYPES_H_HEADER_INCLUDED_
