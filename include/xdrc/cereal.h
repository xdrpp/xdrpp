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

template<typename Archive, typename T> typename
std::enable_if<xdr_class<T>::value>::type
save(Archive &ar, const T &t)
{
  xdr_class<T>::save(ar, t);
}

template<typename Archive, typename T> typename
std::enable_if<xdr_class<T>::value>::type
load(Archive &ar, T &t)
{
  xdr_class<T>::load(ar, t);
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
std::enable_if<cereal::traits::is_output_serializable<
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
