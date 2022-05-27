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
  static constexpr value_type value = F;
  static constexpr bool has_field = true;

  static constexpr const char *name() { return Name.value; }

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

template<typename S, typename...Field>
requires (std::same_as<S, typename Field::struct_type> && ...)
struct xdr_struct_base
  : detail::xdr_fixed_size_base<typename Field::struct_type...> {
  using struct_type = S;
  static constexpr std::tuple<Field...> fields{};
  static constexpr size_t num_fields = sizeof...(Field);

  static constexpr const bool is_class = true;
  static constexpr const bool is_struct = true;

  static constexpr size_t serial_size(const struct_type &obj) {
    if constexpr (xdr_struct_base::has_fixed_size)
      return xdr_struct_base::fixed_size;
    else
      return (0 + ... + xdr_get_traits<typename Field::field_type>::
	      serial_size(Field{}(obj)));
  }

  template<typename Archive> static void
  load(Archive &ar, struct_type &obj) {
    (void(archive(ar, Field{}(obj), Field{}.name())), ...);
    validate(obj);
  }
  template<typename Archive> static void
  save(Archive &ar, const struct_type &obj) {
    (void(archive(ar, Field{}(obj), Field{}.name())), ...);
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
    return get<field_type>(s);
  }
  constexpr decltype(auto) operator()(const struct_type &s) const {
    return get<field_type>(s);
  }
  constexpr decltype(auto) operator()(struct_type &&s) const {
    return get<field_type>(std::move(s));
  }
};

} // namespace detail

template<typename...Ts>
struct xdr_traits<std::tuple<Ts...>>
// This is kind of gross, but we want to create a supertype that is
// detail::xdr_struct_base with a sequence of tuple_access template
// parameters from 0 to the size of the tuple.
  : decltype([]<size_t...Is>(std::index_sequence<Is...>){
      return xdr_struct_base<
	std::tuple<Ts...>,
	detail::tuple_access<std::tuple<Ts...>, Is>...
	>{};
  }(std::make_index_sequence<sizeof...(Ts)>{}))
{
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
  static constexpr value_type value = Field;

  static constexpr const char *name() { return Name.value; }

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

template<std::size_t I, typename R, typename F>
inline constexpr R with_integral_constant(F f)
{
  return std::forward<F>(f)(std::integral_constant<std::size_t, I>{});
}

// Invoke an object with a compile-time constant corresponding to the
// runtime value of n.  Template parameter N is the bound on n, which
// must reside in the half-open range [0,N).
template<std::size_t N, typename R = void, typename F>
inline constexpr R
with_n(int n, F &&f)
{
  constexpr auto invoke_array = []<std::size_t...I>(std::index_sequence<I...>) {
      return std::array{ detail::with_integral_constant<I, R, F>... };
    }(std::make_index_sequence<N>{});

  return invoke_array.at(n)(std::forward<F>(f));
}

// Invoke an object with the nth element of a tuple.
template<typename R = void, typename T, typename F>
requires requires(T t) { std::tuple_size_v<std::remove_cvref_t<T>>; get<0>(t); }
inline constexpr R
with_nth(T &&t, int n, F &&f)
{
  return with_n<std::tuple_size_v<std::remove_cvref_t<T>>, R>(n, [&](auto i) {
    return std::forward<F>(f)(get<i>(std::forward<T>(t)));
  });
}

} // namespace detail

template<typename U, bool HasDefault, detail::fixed_string Name,
	 typename TagAccess, auto FieldNo, typename...Arm>
struct xdr_union_common;

template<typename U, bool HasDefault, detail::fixed_string UnionName,
	 typename TagType, TagType U::*TagField,
	 detail::fixed_string TagName, auto FieldNo, typename...Arm>
struct xdr_union_common<U, HasDefault, UnionName,
			field_access_t<TagField, TagName>, FieldNo, Arm...>
  : xdr_traits_base {
  using union_type = U;
  friend union_type;
  using tag_type = TagType;

  static constexpr bool is_union = true;
  static constexpr bool is_class = true;
  static constexpr bool has_default_case = HasDefault;
  static constexpr const char *union_name = UnionName.value;
  static constexpr size_t num_arms = 1 + sizeof...(Arm);
  static constexpr int fieldno(tag_type t) { return FieldNo(t); }

  static tag_type get_tag(const union_type &u) {
    return tag(u);
  }
  static bool set_tag(union_type &u, tag_type v) {
    const int oldno = fieldno(tag(u)), newno = fieldno(v);
    if (oldno == newno)
      tag(u) = v;
    else {
      with_current_arm(u, [&u](auto f) { destroy_field(f, u); });
      tag(u) = v;
      with_current_arm(u, [&u](auto f) { construct_field(f, u); });
    }
    return newno >= 0;
  }

  template<typename R = void, typename F>
  static R with_arm(int n, F &&fn) {
    return detail::with_nth<R>(arms, n, [&](auto f){
      return std::forward<F>(fn)(f);
    });
  }
  template<typename F>
  static void with_current_arm(const union_type &u, F &&fn) {
    const int fno = fieldno(tag(u));
    if (fno > 0)
      detail::with_nth(arms, fno, [&](auto f){ std::forward<F>(fn)(f); });
  }

  template<typename S>
  requires std::same_as<std::remove_cvref_t<S>, union_type>
  static union_type &assign(union_type &u, S &&r) {
    const int oldno = fieldno(tag(u)), newno = fieldno(tag(r));
    tag(u) = tag(r);
    if (newno == oldno) {
      if (newno > 0)
	detail::with_nth(arms, newno, [&](auto f) {
	  f(u) = f(std::forward<S>(r));
	});
    }
    else {
      if (oldno > 0)
	detail::with_nth(arms, oldno, [&u](auto f) { destroy_field(f, u); });
      if (newno > 0)
	detail::with_nth(arms, newno, [&u, &r](auto f) {
	  construct_field(f, u, f(std::forward<S>(r)));
	});
    }
    return u;
  }

  static size_t serial_size(const union_type &u) {
    return 4 + with_arm<size_t>(fieldno(tag(u)), [&u](auto f) {
      return XDR_GET_TRAITS(f(u))::serial_size(f(u));
    });
  }

  template<typename Archive> static void
  save(Archive &ar, const union_type &u) {
    const tag_type t = tag(u);
    check_tag(t);
    archive(ar, t, tag.name());
    with_arm(fieldno(t), [&](auto f) { archive(ar, f(u), f.name()); });
  }
  template<typename Archive> static void
  load(Archive &ar, union_type &u) {
    tag_type t = tag(u);
    archive(ar, t, tag.name());
    check_tag(t);
    set_tag(u, t);
    with_arm(fieldno(t), [&](auto f) { archive(ar, f(u), f.name()); });
  }

protected:
  using case_type = typename xdr_traits<tag_type>::case_type;

  static constexpr field_access_t<TagField, TagName> tag{};
  static constexpr std::tuple<void_access_t, Arm...> arms{};

  static inline void check_tag(tag_type t) {
    if constexpr (!has_default_case)
      if (fieldno(t) == -1) [[unlikely]] {
	static const std::string errmsg =
	  std::string("bad value of ") + tag.name() + " in " + union_name;
	throw xdr_bad_discriminant(errmsg);
      }
  }

  template<size_t I>
  static inline void check_field(tag_type t) {
    if (I != fieldno(t)) [[unlikely]] {
      std::string errmsg = std::string("accessed field ") +
	get<I>(arms).name() + " in " + union_name +
	" when " + tag.name() + " is " + std::to_string(t);
      throw xdr_wrong_union(errmsg);
    }
  }

  template<size_t I>
  static decltype(auto) arm(union_type &u) {
    check_field<I>(tag(u));
    return get<I>(arms)(u);
  }
  template<size_t I>
  static decltype(auto) arm(const union_type &u) {
    check_field<I>(tag(u));
    return get<I>(arms)(u);
  }

  static void construct_field(void_access_t, auto&&, auto&&...) {}
  static void destroy_field(void_access_t, auto&&) {}

  template<typename T, auto Field, detail::fixed_string Name, typename...Args>
  static void construct_field(uptr_access_t<T, Field, Name> f,
			      typename decltype(f)::struct_type &s,
			      Args&&...args) {
    s.*f.value = new T(std::forward<Args>(args)...);
  }
  template<typename T, auto Field, detail::fixed_string Name>
  static void destroy_field(uptr_access_t<T, Field, Name> f,
			    typename decltype(f)::struct_type &s) {
    delete static_cast<T*>(s.*f.value);
    s.*f.value = nullptr;
  }

  template<auto F, detail::fixed_string Name, typename...Args>
  static void construct_field(field_access_t<F, Name> f,
			      typename decltype(f)::struct_type &s,
			      Args&&...args) {
    // XXX - could be awkward if constructor throws
    std::construct_at(&f(s), std::forward<Args>(args)...);
  }
  template<auto F, detail::fixed_string Name>
  static void destroy_field(field_access_t<F, Name> f,
			    union_type &s) {
			    //typename decltype(F)::struct_type &s) {
    std::destroy_at(&f(s));
  }
};


////////////////////////////////////////////////////////////////
// Comparison for structs and unions
////////////////////////////////////////////////////////////////

template<xdr_struct T> requires xdr_traits<T>::xdr_defined
constexpr bool
operator==(const T &a, const T &b) noexcept
{
  return apply([&](auto...fields) {
    return ((fields(a) == fields(b)) && ...);
  }, xdr_traits<T>::fields);
}

namespace detail {

template<typename...T> using field_cmp_res_t =
  std::common_comparison_category_t<std::compare_three_way_result_t<T>...>;

} // namespace detail

template<xdr_struct T> requires xdr_traits<T>::xdr_defined
constexpr auto
operator<=>(const T &a, const T &b) noexcept
{
  return std::apply([&a,&b]<typename...F>(F...f){
      detail::field_cmp_res_t<typename F::field_type...> r =
	std::strong_ordering::equal;
      return void(((r = f(a) <=> f(b), r == 0) && ...)), r;
    }, xdr_traits<T>::fields);
}

template<xdr_union T> inline bool
operator==(const T &a, const T &b) noexcept
{
  if (xdr_traits<T>::get_tag(a) != xdr_traits<T>::get_tag(b))
    return false;
  bool r = true;
  xdr_traits<T>::with_current_arm(a, [&](auto body) {
    r = body(a) == body(b);
  });
  return r;
}

template<xdr_union T>
inline std::partial_ordering
operator<=>(const T &a, const T &b) noexcept
{
  if (auto c = xdr_traits<T>::get_tag(a) <=> xdr_traits<T>::get_tag(b); c != 0)
    return c;
  auto r = std::partial_ordering::equivalent;
  xdr_traits<T>::with_current_arm(a, [&](auto body) {
    r = body(a) <=> body(b);
  });
  return r;
}

} // namespace xdr

#endif // !_XDRC_TYPES_H_HEADER_INCLUDED_
