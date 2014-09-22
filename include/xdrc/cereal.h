// -*- C++ -*-

/** \file cereal.h Interface for cereal serealization back ends.  By
 * including this file, you can archive any XDR data structure with
 * cereal. */

#ifndef _XDRC_CEREAL_H_HEADER_INCLUDED_
#define _XDRC_CEREAL_H_HEADER_INCLUDED_ 1

#include <type_traits>
#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/details/traits.hpp>
#include <xdrc/types.h>

namespace xdr {

#if 0
template<typename Archive> struct cereal_adapter {
  template<uint32_t N> static void
    apply(Archive &ar, const char *field, xstring<N> &s) {
    if (field)
      ar(cereal::make_nvp(field, static_cast<std::string &>(s)));
    else
      ar(static_cast<std::string &>(s));
  }
  template<uint32_t N> static void
    apply(Archive &ar, const char *field, const xstring<N> &s) {
    if (field)
      ar(cereal::make_nvp(field, static_cast<const std::string &>(s)));
    else
      ar(static_cast<const std::string &>(s));
  }

  template<typename T, uint32_t N> static void
    apply(Archive &ar, const char *field, xarray<T, N> &a) {
    if (field)
      ar(cereal::make_nvp(field, static_cast<std::array<T, N> &>(a)));
    else
      ar(static_cast<std::array<T, N> &>(a));
  }
  template<typename T, uint32_t N> static void
    apply(Archive &ar, const char *field, const std::array<T, N> &a) {
    if (field)
      ar(cereal::make_nvp(field, static_cast<const std::array<T, N> &>(a)));
    else
      ar(static_cast<const std::array<T, N> &>(a));
  }

  template<typename T, uint32_t N> static void
    apply(Archive &ar, const char *field, xvector<T, N> &a) {
    if (field)
      ar(cereal::make_nvp(field, static_cast<std::vector<T> &>(a)));
    else
      ar(static_cast<std::vector<T> &>(a));
  }
  template<typename T, uint32_t N> static void
    apply(Archive &ar, const char *field, const xvector<T> &a) {
    if (field)
      ar(cereal::make_nvp(field, static_cast<const std::vector<T> &>(a)));
    else
      ar(static_cast<const std::vector<T> &>(a));
  }
};
#endif

template<typename Archive, typename T> typename
std::enable_if<xdr_recursive<T>::value>::type
save(Archive &ar, const T &t)
{
  xdr_recursive<T>::save(ar, t);
}

template<typename Archive, typename T> typename
std::enable_if<xdr_recursive<T>::value>::type
load(Archive &ar, T &t)
{
  xdr_recursive<T>::load(ar, t);
}

template<typename Archive, typename T> typename
std::enable_if<cereal::traits::is_output_serializable<
		 cereal::BinaryData<char *>,Archive>::value
               && xdr_bytes<T>::value>::type
save(Archive &ar, const T &t)
{
  if (xdr_bytes<T>::variable)
    ar(cereal::make_size_tag(static_cast<cereal::size_type>(t.size())));
  ar(cereal::binary_data(const_cast<char *>(
         reinterpret_cast<const char *>(t.data())), t.size()));
}

template<typename Archive, typename T> typename
std::enable_if<cereal::traits::is_input_serializable<
		 cereal::BinaryData<char *>,Archive>::value
               && xdr_bytes<T>::value>::type
load(Archive &ar, T &t)
{
  cereal::size_type size;
  if (xdr_bytes<T>::variable)
    ar(cereal::make_size_tag(size));
  else
    size = t.size();
  t.check_size(size);
  t.resize(static_cast<std::uint32_t>(size));
  ar(cereal::binary_data(t.data(), size));
}

#if 0
//! \hideinitializer
#define XDR_ARCHIVE_TAKES_NAME(archive)					\
} namespace cereal { class archive; } namespace xdr {			\
template<> struct prepare_field<cereal::archive> {			\
  template<typename T> static inline cereal::NameValuePair<T>		\
  prepare(const char *name, T &&t) {					\
    return cereal::make_nvp(name, std::forward<T>(t));			\
  }									\
  template<std::uint32_t N>						\
  static inline cereal::NameValuePair<const std::string &>		\
  prepare(const char *name, const xstring<N> &s) {			\
    return cereal::make_nvp(name, static_cast<const std::string &>(s));	\
  }									\
  template<std::uint32_t N>						\
  static inline cereal::NameValuePair<std::string &>			\
  prepare(const char *name, xstring<N> &s) {				\
    return cereal::make_nvp(name, static_cast<std::string &>(s));	\
  }									\
};

XDR_ARCHIVE_TAKES_NAME(JSONInputArchive)
XDR_ARCHIVE_TAKES_NAME(JSONOutputArchive)
XDR_ARCHIVE_TAKES_NAME(XMLOutputArchive)
XDR_ARCHIVE_TAKES_NAME(XMLInputArchive)
#undef XDR_ARCHIVE_TAKES_NAME
#endif

}

namespace cereal {

using xdr::load;
using xdr::save;

}

#endif // !_XDRC_CEREAL_H_HEADER_INCLUDED_
