// -*- C++ -*-

/** \file cereal.h Interface for cereal serealization back ends. */

#ifndef _XDRC_CEREAL_H_HEADER_INCLUDED_
#define _XDRC_CEREAL_H_HEADER_INCLUDED_ 1

#include <type_traits>
#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/details/traits.hpp>
#include <xdrc/types.h>

namespace xdr {

template<typename Archive, uint32_t N> inline
typename std::enable_if<cereal::traits::is_output_serializable<
			  cereal::BinaryData<char *>,Archive>::value>::type
save(Archive &ar, const xstring<N> &s)
{
  s.validate();
  ar(cereal::make_size_tag(static_cast<cereal::size_type>(s.size())));
  ar(cereal::binary_data(const_cast<char *>(s.data()), s.size()));
}

template<typename Archive, uint32_t N> inline
typename std::enable_if<cereal::traits::is_input_serializable<
			  cereal::BinaryData<char *>,Archive>::value>::type
load(Archive &ar, xstring<N> &s)
{
  cereal::size_type size;
  ar(cereal::make_size_tag(size));
  s.check_size(size);
  s.resize(static_cast<std::size_t>(size));
  ar(cereal::binary_data(&s[0], size));
}


#define XDR_ARCHIVE_TAKES_NAME(archive)					\
} namespace cereal { class archive; } namespace xdr {			\
template<> struct prepare_field<cereal::archive> {			\
  template<typename T> static inline cereal::NameValuePair<T>		\
  nvp(const char *name, T &&t) {					\
    return cereal::make_nvp(name, std::forward<T>(t));			\
  }									\
  template<std::uint32_t N>						\
  static inline cereal::NameValuePair<const std::string &>		\
  nvp(const char *name, const xstring<N> &s) {				\
    s.validate();							\
    return cereal::make_nvp(name, static_cast<const std::string &>(s));	\
  }									\
  template<std::uint32_t N>						\
  static inline cereal::NameValuePair<std::string &>			\
  nvp(const char *name, xstring<N> &s) {				\
    return cereal::make_nvp(name, static_cast<std::string &>(s));	\
    /* XXX - no way to validate	*/					\
  }									\
};

XDR_ARCHIVE_TAKES_NAME(JSONInputArchive)
XDR_ARCHIVE_TAKES_NAME(JSONOutputArchive)
XDR_ARCHIVE_TAKES_NAME(XMLOutputArchive)
XDR_ARCHIVE_TAKES_NAME(XMLInputArchive)
#undef XDR_ARCHIVE_TAKES_NAME

}

#endif // !_XDRC_CEREAL_H_HEADER_INCLUDED_
